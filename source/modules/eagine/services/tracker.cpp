/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.services:tracker;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.build_info;
import eagine.core.identifier;
import eagine.core.utility;
import eagine.core.units;
import eagine.core.valid_if;
import eagine.core.main_ctx;
import eagine.msgbus.core;
import :common_info;
import :discovery;
import :host_info;
import :ping_pong;
import :statistics;
import :system_info;
import :topology;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Alias for base service composition for the message bus node tracker.
/// @ingroup msgbus
/// @see node_tracker
/// @see service_composition
export template <typename Base>
using node_tracker_base = require_services<
  Base,
  pinger,
  system_info_consumer,
  common_info_consumers,
  statistics_consumer,
  network_topology,
  subscriber_discovery>;
//------------------------------------------------------------------------------
/// @brief Service that consumes bus topology information and provides it via an API.
/// @ingroup msgbus
/// @see service_composition
///
/// This class subscribes to the signals inherited from the node_tracker_base
/// and tracks the information about the message bus topology, routers, bridges
/// and endpoints, etc.
export template <typename Base = subscriber>
class node_tracker : public node_tracker_base<Base> {

    using This = node_tracker;
    using base = node_tracker_base<Base>;

public:
    /// @brief Triggered when message bus host information changes.
    signal<void(remote_host&, const remote_host_changes) noexcept> host_changed;

    /// @brief Triggered when message bus instance information changes.
    signal<void(remote_instance&, const remote_instance_changes) noexcept>
      instance_changed;

    /// @brief Triggered when message bus node information changes.
    signal<void(remote_node&, const remote_node_changes) noexcept> node_changed;

    auto update() noexcept -> work_done {
        some_true something_done{};
        something_done(base::update());

        if(_should_query_topology) {
            this->discover_topology();
            something_done();
        }

        if(_should_query_stats) {
            this->discover_statistics();
            something_done();
        }

        const bool should_query_info{_should_query_info};

        _tracker.for_each_host_state([&](const auto host_id, auto& host) {
            _handle_host_change(host_id, host);
        });

        _tracker.for_each_instance_state([&](const auto inst_id, auto& inst) {
            _handle_inst_change(inst_id, inst);
        });

        _tracker.for_each_node_state([&](const auto node_id, auto& node) {
            if(should_query_info) {
                if(!node.has_known_kind()) {
                    this->query_topology(node_id);
                }
                if(!node.host_id()) {
                    this->query_host_id(node_id);
                }
                if(!node.has_endpoint_info()) {
                    this->query_endpoint_info(node_id);
                }
                if(!node.instance().compiler()) {
                    this->query_compiler_info(node_id);
                }
                if(!node.instance().build_version()) {
                    this->query_build_version_info(node_id);
                }
                if(node.is_responsive()) {
                    if(const auto inst{node.instance_state()}) {
                        if(!inst.application_name()) {
                            this->query_application_name(node_id);
                        }
                    }
                    if(auto host{node.host_state()}) {
                        if(!host.name()) {
                            this->query_hostname(node_id);
                        }
                        if(!host.cpu_concurrent_threads()) {
                            this->query_cpu_concurrent_threads(node_id);
                        }
                        if(!host.total_ram_size()) {
                            this->query_total_ram_size(node_id);
                        }
                        if(!host.total_swap_size()) {
                            this->query_total_swap_size(node_id);
                        }

                        if(node.can_query_system_info()) {
                            if(host.should_query_sensors()) {
                                this->query_sensors(node_id);
                                host.sensors_queried();
                            }
                        }
                    }
                }
            }

            if(node.is_pingable()) {
                const auto [should_ping, max_time] = node.should_ping();
                if(should_ping) {
                    this->ping(node_id, max_time);
                    node.pinged();
                    something_done();
                }
            }
            _handle_node_change(node_id, node);
        });

        return something_done;
    }

    /// @brief Calls the specified function for each tracked node.
    template <typename Function>
    void for_each_node(Function function) {
        _tracker.for_each_node(std::move(function));
    }

    /// @brief Returns information about a host with the specified id.
    auto get_host(const host_id_t id) noexcept -> const remote_host& {
        return _tracker.get_host(id);
    }

    /// @brief Returns information about an instance with the specified id.
    auto get_instance(const process_instance_id_t id) noexcept
      -> const remote_instance& {
        return _tracker.get_instance(id);
    }

    /// @brief Returns information about a node with the specified id.
    auto get_node(const identifier_t id) noexcept -> const remote_node& {
        return _tracker.get_node(id);
    }

protected:
    using base::base;

    void init() noexcept {
        base::init();

        connect<&This::_handle_alive>(this, this->reported_alive);
        connect<&This::_handle_subscribed>(this, this->subscribed);
        connect<&This::_handle_unsubscribed>(this, this->unsubscribed);
        connect<&This::_handle_not_subscribed>(this, this->not_subscribed);
        connect<&This::_handle_host_id_received>(this, this->host_id_received);
        connect<&This::_handle_hostname_received>(
          this, this->hostname_received);
        connect<&This::_handle_router_appeared>(this, this->router_appeared);
        connect<&This::_handle_bridge_appeared>(this, this->bridge_appeared);
        connect<&This::_handle_endpoint_appeared>(
          this, this->endpoint_appeared);
        connect<&This::_handle_router_disappeared>(
          this, this->router_disappeared);
        connect<&This::_handle_bridge_disappeared>(
          this, this->bridge_disappeared);
        connect<&This::_handle_endpoint_disappeared>(
          this, this->endpoint_disappeared);
        connect<&This::_handle_router_stats_received>(
          this, this->router_stats_received);
        connect<&This::_handle_bridge_stats_received>(
          this, this->bridge_stats_received);
        connect<&This::_handle_endpoint_stats_received>(
          this, this->endpoint_stats_received);
        connect<&This::_handle_connection_stats_received>(
          this, this->connection_stats_received);
        connect<&This::_handle_application_name_received>(
          this, this->application_name_received);
        connect<&This::_handle_endpoint_info_received>(
          this, this->endpoint_info_received);
        connect<&This::_handle_compiler_info_received>(
          this, this->compiler_info_received);
        connect<&This::_handle_build_version_info_received>(
          this, this->build_version_info_received);
        connect<&This::_handle_cpu_concurrent_threads_received>(
          this, this->cpu_concurrent_threads_received);
        connect<&This::_handle_short_average_load_received>(
          this, this->short_average_load_received);
        connect<&This::_handle_long_average_load_received>(
          this, this->long_average_load_received);
        connect<&This::_handle_free_ram_size_received>(
          this, this->free_ram_size_received);
        connect<&This::_handle_total_ram_size_received>(
          this, this->total_ram_size_received);
        connect<&This::_handle_free_swap_size_received>(
          this, this->free_swap_size_received);
        connect<&This::_handle_total_swap_size_received>(
          this, this->total_swap_size_received);
        connect<&This::_handle_temperature_min_max_received>(
          this, this->temperature_min_max_received);
        connect<&This::_handle_power_supply_kind_received>(
          this, this->power_supply_kind_received);
        connect<&This::_handle_ping_response>(this, this->ping_responded);
        connect<&This::_handle_ping_timeout>(this, this->ping_timeouted);
    }

    void add_methods() noexcept {
        base::add_methods();
    }

private:
    resetting_timeout _should_query_topology{std::chrono::seconds{15}, nothing};
    resetting_timeout _should_query_stats{std::chrono::seconds{30}, nothing};
    resetting_timeout _should_query_info{std::chrono::seconds{5}};

    remote_node_tracker _tracker{};

    auto _get_host(const host_id_t id) noexcept -> remote_host_state& {
        return _tracker.get_host(id);
    }

    auto _get_instance(const process_instance_id_t id) noexcept
      -> remote_instance_state& {
        return _tracker.get_instance(id);
    }

    auto _get_node(const identifier_t id) noexcept -> remote_node_state& {
        return _tracker.get_node(id);
    }

    auto _get_connection(const identifier_t id1, const identifier_t id2) noexcept
      -> node_connection_state& {
        return _tracker.get_connection(id1, id2);
    }

    void _handle_host_change(
      const identifier_t,
      remote_host_state& host) noexcept {
        if(const auto changes{host.update().changes()}) {
            host_changed(host, changes);
        }
    }

    void _handle_inst_change(
      const identifier_t,
      remote_instance_state& inst) noexcept {
        if(const auto changes{inst.update().changes()}) {
            instance_changed(inst, changes);
        }
    }

    void _handle_node_change(
      const identifier_t node_id,
      remote_node_state& node) noexcept {
        if(const auto changes{node.update().changes()}) {
            node_changed(node, changes);
            if(changes.new_instance()) [[unlikely]] {
                this->query_endpoint_info(node_id);
                this->query_host_id(node_id);
                this->query_hostname(node_id);
                this->query_subscriptions_of(node_id);
            }
        }
    }

    void _handle_alive(const subscriber_info& info) noexcept {
        _tracker.notice_instance(info.endpoint_id, info.instance_id)
          .assign(node_kind::endpoint);
    }

    void _handle_subscribed(
      const subscriber_info& info,
      const message_id msg_id) noexcept {
        _tracker.notice_instance(info.endpoint_id, info.instance_id)
          .add_subscription(msg_id);
    }

    void _handle_unsubscribed(
      const subscriber_info& info,
      const message_id msg_id) noexcept {
        _tracker.notice_instance(info.endpoint_id, info.instance_id)
          .remove_subscription(msg_id);
    }

    void _handle_not_subscribed(
      const subscriber_info& info,
      const message_id msg_id) noexcept {
        _tracker.notice_instance(info.endpoint_id, info.instance_id)
          .remove_subscription(msg_id);
    }

    void _handle_router_appeared(const router_topology_info& info) noexcept {
        _tracker.notice_instance(info.router_id, info.instance_id)
          .assign(node_kind::router);
        if(info.remote_id) {
            _get_connection(info.router_id, info.remote_id)
              .set_kind(info.connect_kind);
        }
    }

    void _handle_bridge_appeared(const bridge_topology_info& info) noexcept {
        _tracker.notice_instance(info.bridge_id, info.instance_id)
          .assign(node_kind::bridge);
        if(info.opposite_id) {
            _get_connection(info.bridge_id, info.opposite_id)
              .set_kind(connection_kind::remote_interprocess);
        }
    }

    void _handle_endpoint_appeared(const endpoint_topology_info& info) noexcept {
        _tracker.notice_instance(info.endpoint_id, info.instance_id)
          .assign(node_kind::endpoint);
    }

    void _handle_router_disappeared(const identifier_t router_id) noexcept {
        _tracker.remove_node(router_id);
    }

    void _handle_bridge_disappeared(const identifier_t bridge_id) noexcept {
        _tracker.remove_node(bridge_id);
    }

    void _handle_endpoint_disappeared(const identifier_t endpoint_id) noexcept {
        _tracker.remove_node(endpoint_id);
    }

    void _handle_router_stats_received(
      const identifier_t router_id,
      const router_statistics& stats) noexcept {
        _get_node(router_id).assign(stats).notice_alive();
    }

    void _handle_bridge_stats_received(
      const identifier_t bridge_id,
      const bridge_statistics& stats) noexcept {
        _get_node(bridge_id).assign(stats).notice_alive();
    }

    void _handle_endpoint_stats_received(
      const identifier_t endpoint_id,
      const endpoint_statistics& stats) noexcept {
        _get_node(endpoint_id).assign(stats).notice_alive();
    }

    void _handle_connection_stats_received(
      const connection_statistics& stats) noexcept {
        _get_connection(stats.local_id, stats.remote_id);
    }

    void _handle_application_name_received(
      const result_context& ctx,
      const valid_if_not_empty<std::string>& app_name) noexcept {
        if(app_name) {
            auto& node = _get_node(ctx.source_id());
            if(const auto inst_id{node.instance_id()}) {
                auto& inst = _get_instance(extract(inst_id));
                inst.set_app_name(extract(app_name)).notice_alive();
                _tracker.for_each_instance_node_state(
                  extract(inst_id), [&](auto, auto& inst_node) {
                      inst_node.add_change(
                        remote_node_change::application_info);
                  });
            }
        }
    }

    void _handle_endpoint_info_received(
      const result_context& ctx,
      const endpoint_info& info) noexcept {
        _get_node(ctx.source_id()).assign(info).notice_alive();
    }

    void _handle_host_id_received(
      const result_context& ctx,
      const valid_if_positive<host_id_t>& host_id) noexcept {
        if(host_id) {
            _get_node(ctx.source_id())
              .set_host_id(extract(host_id))
              .notice_alive();
        }
    }

    void _handle_hostname_received(
      const result_context& ctx,
      const valid_if_not_empty<std::string>& hostname) noexcept {
        if(hostname) {
            auto& node = _get_node(ctx.source_id());
            if(const auto host_id{node.host_id()}) {
                auto& host = _get_host(extract(host_id));
                host.set_hostname(extract(hostname)).notice_alive();
                _tracker.for_each_host_node_state(
                  extract(host_id), [&](auto, auto& host_node) {
                      host_node.add_change(remote_node_change::host_info);
                  });
            }
        }
    }

    void _handle_compiler_info_received(
      const result_context& ctx,
      const compiler_info& info) noexcept {
        auto& node = _get_node(ctx.source_id()).notice_alive();
        if(const auto inst_id{node.instance_id()}) {
            auto& inst = _get_instance(extract(inst_id));
            inst.assign(info);
            _tracker.for_each_instance_node_state(
              extract(inst_id), [&](auto, auto& inst_node) {
                  inst_node.add_change(remote_node_change::build_info);
              });
        }
    }

    void _handle_build_version_info_received(
      const result_context& ctx,
      const version_info& info) noexcept {
        auto& node = _get_node(ctx.source_id()).notice_alive();
        if(const auto inst_id{node.instance_id()}) {
            auto& inst = _get_instance(extract(inst_id));
            inst.assign(info);
            _tracker.for_each_instance_node_state(
              extract(inst_id), [&](auto, auto& inst_node) {
                  inst_node.add_change(remote_node_change::build_info);
              });
        }
    }

    void _handle_cpu_concurrent_threads_received(
      const result_context& ctx,
      const valid_if_positive<span_size_t>& opt_value) noexcept {
        if(opt_value) {
            auto& node = _get_node(ctx.source_id()).notice_alive();
            if(const auto host_id{node.host_id()}) {
                auto& host = _get_host(extract(host_id)).notice_alive();
                host.set_cpu_concurrent_threads(extract(opt_value));
                _tracker.for_each_host_node_state(
                  extract(host_id), [&](auto, auto& host_node) {
                      host_node.add_change(remote_node_change::hardware_config);
                  });
            }
        }
    }

    void _handle_short_average_load_received(
      const result_context& ctx,
      const valid_if_nonnegative<float>& opt_value) noexcept {
        if(opt_value) {
            auto& node = _get_node(ctx.source_id()).notice_alive();
            if(const auto host_id{node.host_id()}) {
                auto& host = _get_host(extract(host_id)).notice_alive();
                host.set_short_average_load(extract(opt_value));
                _tracker.for_each_host_node_state(
                  extract(host_id), [&](auto, auto& host_node) {
                      host_node.add_change(remote_node_change::sensor_values);
                  });
            }
        }
    }

    void _handle_long_average_load_received(
      const result_context& ctx,
      const valid_if_nonnegative<float>& opt_value) noexcept {
        if(opt_value) {
            auto& node = _get_node(ctx.source_id()).notice_alive();
            if(const auto host_id{node.host_id()}) {
                auto& host = _get_host(extract(host_id)).notice_alive();
                host.set_long_average_load(extract(opt_value));
                _tracker.for_each_host_node_state(
                  extract(host_id), [&](auto, auto& host_node) {
                      host_node.add_change(remote_node_change::sensor_values);
                  });
            }
        }
    }

    void _handle_free_ram_size_received(
      const result_context& ctx,
      const valid_if_positive<span_size_t>& opt_value) noexcept {
        if(opt_value) {
            auto& node = _get_node(ctx.source_id()).notice_alive();
            if(const auto host_id{node.host_id()}) {
                auto& host = _get_host(extract(host_id)).notice_alive();
                host.set_free_ram_size(extract(opt_value));
                _tracker.for_each_host_node_state(
                  extract(host_id), [&](auto, auto& host_node) {
                      host_node.add_change(remote_node_change::sensor_values);
                  });
            }
        }
    }

    void _handle_total_ram_size_received(
      const result_context& ctx,
      const valid_if_positive<span_size_t>& opt_value) noexcept {
        if(opt_value) {
            auto& node = _get_node(ctx.source_id()).notice_alive();
            if(const auto host_id{node.host_id()}) {
                auto& host = _get_host(extract(host_id)).notice_alive();
                host.set_total_ram_size(extract(opt_value));
                _tracker.for_each_host_node_state(
                  extract(host_id), [&](auto, auto& host_node) {
                      host_node.add_change(remote_node_change::hardware_config);
                  });
            }
        }
    }

    void _handle_free_swap_size_received(
      const result_context& ctx,
      const valid_if_nonnegative<span_size_t>& opt_value) noexcept {
        if(opt_value) {
            auto& node = _get_node(ctx.source_id()).notice_alive();
            if(const auto host_id{node.host_id()}) {
                auto& host = _get_host(extract(host_id)).notice_alive();
                host.set_free_swap_size(extract(opt_value));
                _tracker.for_each_host_node_state(
                  extract(host_id), [&](auto, auto& host_node) {
                      host_node.add_change(remote_node_change::sensor_values);
                  });
            }
        }
    }

    void _handle_total_swap_size_received(
      const result_context& ctx,
      const valid_if_nonnegative<span_size_t>& opt_value) noexcept {
        if(opt_value) {
            auto& node = _get_node(ctx.source_id()).notice_alive();
            if(const auto host_id{node.host_id()}) {
                auto& host = _get_host(extract(host_id)).notice_alive();
                host.set_total_swap_size(extract(opt_value));
                _tracker.for_each_host_node_state(
                  extract(host_id), [&](auto, auto& host_node) {
                      host_node.add_change(remote_node_change::hardware_config);
                  });
            }
        }
    }

    void _handle_temperature_min_max_received(
      const result_context& ctx,
      const std::tuple<
        valid_if_positive<kelvins_t<float>>,
        valid_if_positive<kelvins_t<float>>>& value) noexcept {
        const auto& [min, max] = value;
        if(min && max) {
            auto& node = _get_node(ctx.source_id()).notice_alive();
            if(const auto host_id{node.host_id()}) {
                auto& host = _get_host(extract(host_id)).notice_alive();
                host.set_temperature_min_max(extract(min), extract(max));
                _tracker.for_each_host_node_state(
                  extract(host_id), [&](auto, auto& host_node) {
                      host_node.add_change(remote_node_change::sensor_values);
                  });
            }
        }
    }

    void _handle_power_supply_kind_received(
      const result_context& ctx,
      const power_supply_kind value) noexcept {
        auto& node = _get_node(ctx.source_id()).notice_alive();
        if(const auto host_id{node.host_id()}) {
            auto& host = _get_host(extract(host_id)).notice_alive();
            host.set_power_supply(value);
            _tracker.for_each_host_node_state(
              extract(host_id), [&](auto, auto& host_node) {
                  host_node.add_change(remote_node_change::sensor_values);
              });
        }
    }

    void _handle_ping_response(
      const identifier_t node_id,
      const message_sequence_t sequence_no,
      const std::chrono::microseconds age,
      const verification_bits) noexcept {
        _get_node(node_id).ping_response(sequence_no, age);
    }

    void _handle_ping_timeout(
      const identifier_t node_id,
      const message_sequence_t sequence_no,
      const std::chrono::microseconds age) noexcept {
        _get_node(node_id).ping_timeout(sequence_no, age);
    }
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


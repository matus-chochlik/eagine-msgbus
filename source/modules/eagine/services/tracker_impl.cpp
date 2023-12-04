/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module eagine.msgbus.services;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.units;
import eagine.core.utility;
import eagine.core.valid_if;
import eagine.core.build_info;
import eagine.core.main_ctx;
import eagine.msgbus.core;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
class node_tracker_impl : public node_tracker_intf {
    using This = node_tracker_impl;

public:
    node_tracker_impl(subscriber& sub, node_tracker_signals& sigs) noexcept
      : base{sub}
      , signals{sigs} {}

    void init(
      pinger_signals& pings,
      system_info_consumer_signals& system,
      compiler_info_consumer_signals& compiler,
      build_version_info_consumer_signals& build_version,
      host_info_consumer_signals& host,
      application_info_consumer_signals& application,
      endpoint_info_consumer_signals& bus_endpoint,
      statistics_consumer_signals& statistics,
      network_topology_signals& topology,
      subscriber_discovery_signals& discovery) noexcept final {
        connect<&This::_handle_alive>(this, discovery.reported_alive);
        connect<&This::_handle_subscribed>(this, discovery.subscribed);
        connect<&This::_handle_unsubscribed>(this, discovery.unsubscribed);
        connect<&This::_handle_not_subscribed>(this, discovery.not_subscribed);
        connect<&This::_handle_host_id_received>(this, host.host_id_received);
        connect<&This::_handle_hostname_received>(this, host.hostname_received);
        connect<&This::_handle_router_appeared>(this, topology.router_appeared);
        connect<&This::_handle_bridge_appeared>(this, topology.bridge_appeared);
        connect<&This::_handle_endpoint_appeared>(
          this, topology.endpoint_appeared);
        connect<&This::_handle_router_disappeared>(
          this, topology.router_disappeared);
        connect<&This::_handle_bridge_disappeared>(
          this, topology.bridge_disappeared);
        connect<&This::_handle_endpoint_disappeared>(
          this, topology.endpoint_disappeared);
        connect<&This::_handle_router_stats_received>(
          this, statistics.router_stats_received);
        connect<&This::_handle_bridge_stats_received>(
          this, statistics.bridge_stats_received);
        connect<&This::_handle_endpoint_stats_received>(
          this, statistics.endpoint_stats_received);
        connect<&This::_handle_connection_stats_received>(
          this, statistics.connection_stats_received);
        connect<&This::_handle_application_name_received>(
          this, application.application_name_received);
        connect<&This::_handle_endpoint_info_received>(
          this, bus_endpoint.endpoint_info_received);
        connect<&This::_handle_compiler_info_received>(
          this, compiler.compiler_info_received);
        connect<&This::_handle_build_version_info_received>(
          this, build_version.build_version_info_received);
        connect<&This::_handle_cpu_concurrent_threads_received>(
          this, system.cpu_concurrent_threads_received);
        connect<&This::_handle_short_average_load_received>(
          this, system.short_average_load_received);
        connect<&This::_handle_long_average_load_received>(
          this, system.long_average_load_received);
        connect<&This::_handle_free_ram_size_received>(
          this, system.free_ram_size_received);
        connect<&This::_handle_total_ram_size_received>(
          this, system.total_ram_size_received);
        connect<&This::_handle_free_swap_size_received>(
          this, system.free_swap_size_received);
        connect<&This::_handle_total_swap_size_received>(
          this, system.total_swap_size_received);
        connect<&This::_handle_temperature_min_max_received>(
          this, system.temperature_min_max_received);
        connect<&This::_handle_power_supply_kind_received>(
          this, system.power_supply_kind_received);
        connect<&This::_handle_ping_response>(this, pings.ping_responded);
        connect<&This::_handle_ping_timeout>(this, pings.ping_timeouted);
    }

    void update(callable_ref<void(const endpoint_id_t, remote_node_state&)>
                  update_node) noexcept final {

        _tracker.for_each_host_state([&](const auto host_id, auto& host) {
            _handle_host_change(host_id, host);
        });

        _tracker.for_each_instance_state([&](const auto inst_id, auto& inst) {
            _handle_inst_change(inst_id, inst);
        });

        _tracker.for_each_node_state([&](auto node_id, auto& node) {
            update_node(node_id, node);
            _handle_node_change(node_id, node);
        });
    }

    void update_node_info(
      callable_ref<void(const endpoint_id_t)> update_node) noexcept final {
        if(not _update_node_ids.empty()) {
            for(const auto node_id : _update_node_ids) {
                update_node(node_id);
            }
            _update_node_ids.clear();
        }
    }

    auto tracker() noexcept -> remote_node_tracker& {
        return _tracker;
    }

    auto should_query_topology() noexcept -> bool {
        return _should_query_topology.is_expired();
    }

    auto should_query_stats() noexcept -> bool {
        return _should_query_stats.is_expired();
    }

    auto should_query_info() noexcept -> bool {
        return _should_query_info.is_expired();
    }

private:
    void _handle_host_change(
      const endpoint_id_t,
      remote_host_state& host) noexcept {
        if(const auto changes{host.update().changes()}) {
            signals.host_changed(host, changes);
        }
    }

    void _handle_inst_change(
      const endpoint_id_t,
      remote_instance_state& inst) noexcept {
        if(const auto changes{inst.update().changes()}) {
            signals.instance_changed(inst, changes);
        }
    }

    void _handle_node_change(
      const endpoint_id_t node_id,
      remote_node_state& node) noexcept {
        if(const auto changes{node.update().changes()}) {
            signals.node_changed(node, changes);
            if(changes.new_instance()) [[unlikely]] {
                _update_node_ids.push_back(node_id);
            }
        }
    }

    void _handle_alive(
      const result_context&,
      const subscriber_alive& alive) noexcept {
        _tracker
          .notice_instance(alive.source.endpoint_id, alive.source.instance_id)
          .assign(node_kind::endpoint);
    }

    void _handle_subscribed(
      const result_context&,
      const subscriber_subscribed& sub) noexcept {
        _tracker.notice_instance(sub.source.endpoint_id, sub.source.instance_id)
          .add_subscription(sub.message_type);
    }

    void _handle_unsubscribed(
      const result_context&,
      const subscriber_unsubscribed& sub) noexcept {
        _tracker.notice_instance(sub.source.endpoint_id, sub.source.instance_id)
          .remove_subscription(sub.message_type);
    }

    void _handle_not_subscribed(
      const result_context&,
      const subscriber_not_subscribed& sub) noexcept {
        _tracker.notice_instance(sub.source.endpoint_id, sub.source.instance_id)
          .remove_subscription(sub.message_type);
    }

    void _handle_router_appeared(
      const result_context&,
      const router_topology_info& info) noexcept {
        _tracker.notice_instance(info.router_id, info.instance_id)
          .assign(node_kind::router);
        if(info.remote_id) {
            _get_connection(info.router_id, info.remote_id)
              .set_kind(info.connect_kind);
        }
    }

    void _handle_bridge_appeared(
      const result_context&,
      const bridge_topology_info& info) noexcept {
        _tracker.notice_instance(info.bridge_id, info.instance_id)
          .assign(node_kind::bridge);
        if(info.opposite_id) {
            _get_connection(info.bridge_id, info.opposite_id)
              .set_kind(connection_kind::remote_interprocess);
        }
    }

    void _handle_endpoint_appeared(
      const result_context&,
      const endpoint_topology_info& info) noexcept {
        _tracker.notice_instance(info.endpoint_id, info.instance_id)
          .assign(node_kind::endpoint);
    }

    void _handle_router_disappeared(
      const result_context&,
      const router_shutdown& info) noexcept {
        _tracker.remove_node(info.router_id);
    }

    void _handle_bridge_disappeared(
      const result_context&,
      const bridge_shutdown& info) noexcept {
        _tracker.remove_node(info.bridge_id);
    }

    void _handle_endpoint_disappeared(
      const result_context&,
      const endpoint_shutdown& info) noexcept {
        _tracker.remove_node(info.endpoint_id);
    }

    void _handle_router_stats_received(
      const result_context& ctx,
      const router_statistics& stats) noexcept {
        _get_node(ctx.source_id()).assign(stats).notice_alive();
    }

    void _handle_bridge_stats_received(
      const result_context& ctx,
      const bridge_statistics& stats) noexcept {
        _get_node(ctx.source_id()).assign(stats).notice_alive();
    }

    void _handle_endpoint_stats_received(
      const result_context& ctx,
      const endpoint_statistics& stats) noexcept {
        _get_node(ctx.source_id()).assign(stats).notice_alive();
    }

    void _handle_connection_stats_received(
      const result_context&,
      const connection_statistics& stats) noexcept {
        _get_connection(stats.local_id, stats.remote_id);
    }

    void _handle_application_name_received(
      const result_context& ctx,
      const valid_if_not_empty<std::string>& app_name) noexcept {
        if(app_name) {
            auto& node = _get_node(ctx.source_id());
            if(const auto inst_id{node.instance_id()}) {
                auto& inst = _get_instance(*inst_id);
                inst.set_app_name(*app_name).notice_alive();
                _tracker.for_each_instance_node_state(
                  *inst_id, [&](auto, auto& inst_node) {
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
            _get_node(ctx.source_id()).set_host_id(*host_id).notice_alive();
        }
    }

    void _handle_hostname_received(
      const result_context& ctx,
      const valid_if_not_empty<std::string>& hostname) noexcept {
        if(hostname) {
            auto& node = _get_node(ctx.source_id());
            if(const auto host_id{node.host_id()}) {
                auto& host = _get_host(*host_id);
                host.set_hostname(*hostname).notice_alive();
                _tracker.for_each_host_node_state(
                  *host_id, [&](auto, auto& host_node) {
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
            auto& inst = _get_instance(*inst_id);
            inst.assign(info);
            _tracker.for_each_instance_node_state(
              *inst_id, [&](auto, auto& inst_node) {
                  inst_node.add_change(remote_node_change::build_info);
              });
        }
    }

    void _handle_build_version_info_received(
      const result_context& ctx,
      const version_info& info) noexcept {
        auto& node = _get_node(ctx.source_id()).notice_alive();
        if(const auto inst_id{node.instance_id()}) {
            auto& inst = _get_instance(*inst_id);
            inst.assign(info);
            _tracker.for_each_instance_node_state(
              *inst_id, [&](auto, auto& inst_node) {
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
                auto& host = _get_host(*host_id).notice_alive();
                host.set_cpu_concurrent_threads(*opt_value);
                _tracker.for_each_host_node_state(
                  *host_id, [&](auto, auto& host_node) {
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
                auto& host = _get_host(*host_id).notice_alive();
                host.set_short_average_load(*opt_value);
                _tracker.for_each_host_node_state(
                  *host_id, [&](auto, auto& host_node) {
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
                auto& host = _get_host(*host_id).notice_alive();
                host.set_long_average_load(*opt_value);
                _tracker.for_each_host_node_state(
                  *host_id, [&](auto, auto& host_node) {
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
                auto& host = _get_host(*host_id).notice_alive();
                host.set_free_ram_size(*opt_value);
                _tracker.for_each_host_node_state(
                  *host_id, [&](auto, auto& host_node) {
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
                auto& host = _get_host(*host_id).notice_alive();
                host.set_total_ram_size(*opt_value);
                _tracker.for_each_host_node_state(
                  *host_id, [&](auto, auto& host_node) {
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
                auto& host = _get_host(*host_id).notice_alive();
                host.set_free_swap_size(*opt_value);
                _tracker.for_each_host_node_state(
                  *host_id, [&](auto, auto& host_node) {
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
                auto& host = _get_host(*host_id).notice_alive();
                host.set_total_swap_size(*opt_value);
                _tracker.for_each_host_node_state(
                  *host_id, [&](auto, auto& host_node) {
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
        if(min and max) {
            auto& node = _get_node(ctx.source_id()).notice_alive();
            if(const auto host_id{node.host_id()}) {
                auto& host = _get_host(*host_id).notice_alive();
                host.set_temperature_min_max(*min, *max);
                _tracker.for_each_host_node_state(
                  *host_id, [&](auto, auto& host_node) {
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
            auto& host = _get_host(*host_id).notice_alive();
            host.set_power_supply(value);
            _tracker.for_each_host_node_state(
              *host_id, [&](auto, auto& host_node) {
                  host_node.add_change(remote_node_change::sensor_values);
              });
        }
    }

    void _handle_ping_response(
      const result_context&,
      const ping_response& pong) noexcept {
        _get_node(pong.pingable_id).ping_response(pong.sequence_no, pong.age);
    }

    void _handle_ping_timeout(const ping_timeout& fail) noexcept {
        _get_node(fail.pingable_id).ping_timeout(fail.sequence_no, fail.age);
    }

    auto _get_host(const host_id_t id) noexcept -> remote_host_state& {
        return _tracker.get_host(id);
    }

    auto _get_instance(const process_instance_id_t id) noexcept
      -> remote_instance_state& {
        return _tracker.get_instance(id);
    }

    auto _get_node(const endpoint_id_t id) noexcept -> remote_node_state& {
        return _tracker.get_node(id);
    }

    auto _get_connection(
      const endpoint_id_t id1,
      const endpoint_id_t id2) noexcept -> node_connection_state& {
        return _tracker.get_connection(id1, id2);
    }

    subscriber& base;
    node_tracker_signals& signals;

    resetting_timeout _should_query_topology{std::chrono::seconds{15}, nothing};
    resetting_timeout _should_query_stats{std::chrono::seconds{30}, nothing};
    resetting_timeout _should_query_info{std::chrono::seconds{5}};

    std::vector<endpoint_id_t> _update_node_ids;

    remote_node_tracker _tracker{};
};
//------------------------------------------------------------------------------
auto make_node_tracker_impl(subscriber& base, node_tracker_signals& sigs)
  -> unique_holder<node_tracker_intf> {
    return {hold<node_tracker_impl>, base, sigs};
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

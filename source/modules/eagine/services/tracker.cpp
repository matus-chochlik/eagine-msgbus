/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.services:tracker;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.utility;
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
/// @brief Collection of signals emitted from node tracker service.
/// @ingroup msgbus
/// @see node_tracker
export struct node_tracker_signals {
    /// @brief Triggered when message bus host information changes.
    signal<void(remote_host&, const remote_host_changes) noexcept> host_changed;

    /// @brief Triggered when message bus instance information changes.
    signal<void(remote_instance&, const remote_instance_changes) noexcept>
      instance_changed;

    /// @brief Triggered when message bus node information changes.
    signal<void(remote_node&, const remote_node_changes) noexcept> node_changed;
};
//------------------------------------------------------------------------------
struct node_tracker_intf : interface<node_tracker_intf> {
    virtual void init(
      pinger_signals&,
      system_info_consumer_signals&,
      compiler_info_consumer_signals&,
      build_version_info_consumer_signals&,
      host_info_consumer_signals&,
      application_info_consumer_signals&,
      endpoint_info_consumer_signals&,
      statistics_consumer_signals&,
      network_topology_signals&,
      subscriber_discovery_signals&) noexcept = 0;

    virtual void update(
      callable_ref<void(const identifier_t, remote_node_state&)>) noexcept = 0;
    virtual void update_node_info(
      callable_ref<void(const identifier_t)>) noexcept = 0;

    virtual auto tracker() noexcept -> remote_node_tracker& = 0;

    virtual auto should_query_topology() noexcept -> bool = 0;
    virtual auto should_query_stats() noexcept -> bool = 0;
    virtual auto should_query_info() noexcept -> bool = 0;
};
//------------------------------------------------------------------------------
auto make_node_tracker_impl(subscriber& base, node_tracker_signals&)
  -> std::unique_ptr<node_tracker_intf>;
//------------------------------------------------------------------------------
/// @brief Service that consumes bus topology information and provides it via an API.
/// @ingroup msgbus
/// @see service_composition
///
/// This class subscribes to the signals inherited from the node_tracker_base
/// and tracks the information about the message bus topology, routers, bridges
/// and endpoints, etc.
export template <typename Base = subscriber>
class node_tracker
  : public node_tracker_base<Base>
  , public node_tracker_signals {

    using This = node_tracker;
    using base = node_tracker_base<Base>;

public:
    auto update() noexcept -> work_done {
        some_true something_done{base::update()};

        if(_impl->should_query_topology()) {
            this->discover_topology();
            something_done();
        }

        if(_impl->should_query_stats()) {
            this->discover_statistics();
            something_done();
        }

        const bool should_query_info{_impl->should_query_info()};
        const auto update_node = [&](const auto node_id, auto& node) {
            if(should_query_info) {
                if(not node.has_known_kind()) {
                    this->query_topology(node_id);
                }
                if(not node.host_id()) {
                    this->query_host_id(node_id);
                }
                if(not node.has_endpoint_info()) {
                    this->query_endpoint_info(node_id);
                }
                if(not node.instance().compiler()) {
                    this->query_compiler_info(node_id);
                }
                if(not node.instance().build_version()) {
                    this->query_build_version_info(node_id);
                }
                if(node.is_responsive()) {
                    if(const auto inst{node.instance_state()}) {
                        if(not inst.application_name()) {
                            this->query_application_name(node_id);
                        }
                    }
                    if(auto host{node.host_state()}) {
                        if(not host.name()) {
                            this->query_hostname(node_id);
                        }
                        if(not host.cpu_concurrent_threads()) {
                            this->query_cpu_concurrent_threads(node_id);
                        }
                        if(not host.total_ram_size()) {
                            this->query_total_ram_size(node_id);
                        }
                        if(not host.total_swap_size()) {
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
        };
        _impl->update({construct_from, update_node});

        const auto query_node_info = [&](const identifier_t node_id) {
            this->query_endpoint_info(node_id);
            this->query_host_id(node_id);
            this->query_hostname(node_id);
            this->query_subscriptions_of(node_id);
        };
        _impl->update_node_info({construct_from, query_node_info});

        return something_done;
    }

    /// @brief Calls the specified function for each tracked node.
    template <typename Function>
    void for_each_node(Function function) {
        _impl->tracker().for_each_node(std::move(function));
    }

    /// @brief Returns information about a host with the specified id.
    auto get_host(const host_id_t id) noexcept -> const remote_host& {
        return _impl->tracker().get_host(id);
    }

    /// @brief Returns information about an instance with the specified id.
    auto get_instance(const process_instance_id_t id) noexcept
      -> const remote_instance& {
        return _impl->tracker().get_instance(id);
    }

    /// @brief Returns information about a node with the specified id.
    auto get_node(const identifier_t id) noexcept -> const remote_node& {
        return _impl->tracker().get_node(id);
    }

protected:
    using base::base;

    void init() noexcept {
        base::init();
        _impl->init(
          *this, *this, *this, *this, *this, *this, *this, *this, *this, *this);
    }

private:
    const std::unique_ptr<node_tracker_intf> _impl{
      make_node_tracker_impl(*this, *this)};
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


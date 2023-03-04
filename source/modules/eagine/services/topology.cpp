/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.services:topology;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.utility;
import eagine.msgbus.core;
import std;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
struct network_topology_intf : interface<network_topology_intf> {
    virtual void add_methods(subscriber& base) noexcept = 0;

    virtual void query_topology(
      endpoint& bus,
      const identifier_t node_id) noexcept = 0;
};
//------------------------------------------------------------------------------
/// @brief Collection of signals emitted by the network topology service
/// @ingroup msgbus
/// @see network_topology
export struct network_topology_signals {

    /// @brief Triggered on receipt of router node topology information.
    /// @see router_disappeared
    /// @see bridge_appeared
    /// @see endpoint_appeared
    signal<void(const router_topology_info&) noexcept> router_appeared;

    /// @brief Triggered on receipt of bridge node topology information.
    /// @see bridge_disappeared
    /// @see router_appeared
    /// @see endpoint_appeared
    signal<void(const bridge_topology_info&) noexcept> bridge_appeared;

    /// @brief Triggered on receipt of endpoint node topology information.
    /// @see endpoint_disappeared
    /// @see router_appeared
    /// @see bridge_appeared
    signal<void(const endpoint_topology_info&) noexcept> endpoint_appeared;

    /// @brief Triggered on receipt of bye-bye message from a router node.
    /// @see router_appeared
    /// @see bridge_disappeared
    /// @see endpoint_disappeared
    signal<void(const identifier_t) noexcept> router_disappeared;

    /// @brief Triggered on receipt of bye-bye message from a bridge node.
    /// @see bridge_appeared
    /// @see router_disappeared
    /// @see endpoint_disappeared
    signal<void(const identifier_t) noexcept> bridge_disappeared;

    /// @brief Triggered on receipt of bye-bye message from an endpoint node.
    /// @see endpoint_appeared
    /// @see router_disappeared
    /// @see bridge_disappeared
    signal<void(const identifier_t) noexcept> endpoint_disappeared;
};
//------------------------------------------------------------------------------
auto make_network_topology_impl(subscriber& base, network_topology_signals&)
  -> std::unique_ptr<network_topology_intf>;
//------------------------------------------------------------------------------
/// @brief Service observing message bus node network topology.
/// @ingroup msgbus
/// @see service_composition
export template <typename Base = subscriber>
class network_topology
  : public Base
  , public network_topology_signals {

public:
    /// @brief Queries the topology information of the specified bus node.
    /// @see discover_topology
    void query_topology(const identifier_t node_id) noexcept {
        _impl->query_topology(this->bus_node(), node_id);
    }

    /// @brief Broadcasts network topology query to all message bus nodes.
    /// @see query_topology
    void discover_topology() noexcept {
        query_topology(broadcast_endpoint_id());
    }

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();
        _impl->add_methods(*this);
    }

private:
    const std::unique_ptr<network_topology_intf> _impl{
      make_network_topology_impl(*this, *this)};
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


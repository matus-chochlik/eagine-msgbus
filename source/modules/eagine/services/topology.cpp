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
export struct router_shutdown {
    identifier_t router_id{invalid_endpoint_id()};
};
export struct bridge_shutdown {
    identifier_t bridge_id{invalid_endpoint_id()};
};
export struct endpoint_shutdown {
    identifier_t endpoint_id{invalid_endpoint_id()};
};
//------------------------------------------------------------------------------
struct network_topology_intf : interface<network_topology_intf> {
    virtual void add_methods(subscriber& base) noexcept = 0;

    virtual void query_topology(
      endpoint& bus,
      const identifier_t node_id) noexcept = 0;

    virtual auto decode_router_topology_info(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<router_topology_info> = 0;

    virtual auto decode_bridge_topology_info(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<bridge_topology_info> = 0;

    virtual auto decode_endpoint_topology_info(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<endpoint_topology_info> = 0;

    virtual auto decode_router_shutdown(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<router_shutdown> = 0;

    virtual auto decode_bridge_shutdown(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<bridge_shutdown> = 0;

    virtual auto decode_endpoint_shutdown(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<endpoint_shutdown> = 0;
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
    signal<void(const router_shutdown&) noexcept> router_disappeared;

    /// @brief Triggered on receipt of bye-bye message from a bridge node.
    /// @see bridge_appeared
    /// @see router_disappeared
    /// @see endpoint_disappeared
    signal<void(const bridge_shutdown&) noexcept> bridge_disappeared;

    /// @brief Triggered on receipt of bye-bye message from an endpoint node.
    /// @see endpoint_appeared
    /// @see router_disappeared
    /// @see bridge_disappeared
    signal<void(const endpoint_shutdown&) noexcept> endpoint_disappeared;
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

    auto decode_router_topology_info(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<router_topology_info> {
        return _impl->decode_router_topology_info(msg_ctx, message);
    }

    auto decode_bridge_topology_info(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<bridge_topology_info> {
        return _impl->decode_bridge_topology_info(msg_ctx, message);
    }

    auto decode_endpoint_topology_info(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<endpoint_topology_info> {
        return _impl->decode_endpoint_topology_info(msg_ctx, message);
    }

    auto decode_router_shutdown(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<router_shutdown> {
        return _impl->decode_router_shutdown(msg_ctx, message);
    }

    auto decode_bridge_shutdown(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<bridge_shutdown> {
        return _impl->decode_bridge_shutdown(msg_ctx, message);
    }

    auto decode_endpoint_shutdown(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<endpoint_shutdown> {
        return _impl->decode_endpoint_shutdown(msg_ctx, message);
    }

    auto decode(const message_context& msg_ctx, const stored_message& message) {
        return this->decode_chain(
          msg_ctx,
          message,
          *static_cast<Base*>(this),
          *this,
          &network_topology::decode_router_topology_info,
          &network_topology::decode_bridge_topology_info,
          &network_topology::decode_endpoint_topology_info,
          &network_topology::decode_router_shutdown,
          &network_topology::decode_bridge_shutdown,
          &network_topology::decode_endpoint_shutdown);
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


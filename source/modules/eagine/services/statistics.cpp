/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.services:statistics;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.utility;
import eagine.msgbus.core;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Collection of signals emitted by bus node network statistics service.
/// @ingroup msgbus
/// @see statistics_consumer
export struct statistics_consumer_signals {

    /// @brief Triggered on receipt of router node statistics information.
    /// @see router_disappeared
    /// @see bridge_stats_received
    /// @see endpoint_stats_received
    /// @see connection_stats_received
    signal<void(const result_context&, const router_statistics&) noexcept>
      router_stats_received;

    /// @brief Triggered on receipt of bridge node statistics information.
    /// @see bridge_disappeared
    /// @see router_stats_received
    /// @see endpoint_stats_received
    /// @see connection_stats_received
    signal<void(const result_context&, const bridge_statistics&) noexcept>
      bridge_stats_received;

    /// @brief Triggered on receipt of endpoint node statistics information.
    /// @see endpoint_disappeared
    /// @see router_stats_received
    /// @see bridge_stats_received
    /// @see connection_stats_received
    signal<void(const result_context&, const endpoint_statistics&) noexcept>
      endpoint_stats_received;

    /// @brief Triggered on receipt of connection statistics information.
    /// @see router_stats_received
    /// @see bridge_stats_received
    /// @see endpoint_stats_received
    signal<void(const result_context&, const connection_statistics&) noexcept>
      connection_stats_received;
};
//------------------------------------------------------------------------------
struct statistics_consumer_intf : interface<statistics_consumer_intf> {
    virtual void add_methods() noexcept = 0;

    virtual void query_statistics(endpoint_id_t node_id) noexcept = 0;

    virtual auto decode_router_statistics(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<router_statistics> = 0;

    virtual auto decode_bridge_statistics(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<bridge_statistics> = 0;

    virtual auto decode_endpoint_statistics(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<endpoint_statistics> = 0;

    virtual auto decode_connection_statistics(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<connection_statistics> = 0;
};
//------------------------------------------------------------------------------
auto make_statistics_consumer_impl(subscriber&, statistics_consumer_signals&)
  -> unique_holder<statistics_consumer_intf>;
//------------------------------------------------------------------------------
/// @brief Service observing message bus node network statistics.
/// @ingroup msgbus
/// @see service_composition
export template <typename Base = subscriber>
class statistics_consumer
  : public Base
  , public statistics_consumer_signals {

public:
    /// @brief Queries the statistics information of the specified bus node.
    /// @see discover_statistics
    void query_statistics(endpoint_id_t node_id) noexcept {
        _impl->query_statistics(node_id);
    }

    /// @brief Broadcasts network statistics query to all message bus nodes.
    /// @see query_statistics
    void discover_statistics() noexcept {
        query_statistics(broadcast_endpoint_id());
    }

    auto decode_router_statistics(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<router_statistics> {
        return _impl->decode_router_statistics(msg_ctx, message);
    }

    auto decode_bridge_statistics(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<bridge_statistics> {
        return _impl->decode_bridge_statistics(msg_ctx, message);
    }

    auto decode_endpoint_statistics(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<endpoint_statistics> {
        return _impl->decode_endpoint_statistics(msg_ctx, message);
    }

    auto decode_connection_statistics(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<connection_statistics> {
        return _impl->decode_connection_statistics(msg_ctx, message);
    }

    auto decode(const message_context& msg_ctx, const stored_message& message) {
        return this->decode_chain(
          msg_ctx,
          message,
          *static_cast<Base*>(this),
          *this,
          &statistics_consumer::decode_router_statistics,
          &statistics_consumer::decode_bridge_statistics,
          &statistics_consumer::decode_endpoint_statistics,
          &statistics_consumer::decode_connection_statistics);
    }

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();
        _impl->add_methods();
    }

private:
    const unique_holder<statistics_consumer_intf> _impl{
      make_statistics_consumer_impl(*this, *this)};
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


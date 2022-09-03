/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.services:statistics;

import eagine.core.types;
import eagine.core.memory;
import eagine.msgbus.core;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
struct statistics_consumer_intf : interface<statistics_consumer_intf> {
    virtual void add_methods() noexcept = 0;

    virtual void query_statistics(identifier_t node_id) noexcept = 0;
};
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
    signal<void(const identifier_t, const router_statistics&) noexcept>
      router_stats_received;

    /// @brief Triggered on receipt of bridge node statistics information.
    /// @see bridge_disappeared
    /// @see router_stats_received
    /// @see endpoint_stats_received
    /// @see connection_stats_received
    signal<void(const identifier_t, const bridge_statistics&) noexcept>
      bridge_stats_received;

    /// @brief Triggered on receipt of endpoint node statistics information.
    /// @see endpoint_disappeared
    /// @see router_stats_received
    /// @see bridge_stats_received
    /// @see connection_stats_received
    signal<void(const identifier_t, const endpoint_statistics&) noexcept>
      endpoint_stats_received;

    /// @brief Triggered on receipt of connection statistics information.
    /// @see router_stats_received
    /// @see bridge_stats_received
    /// @see endpoint_stats_received
    signal<void(const connection_statistics&) noexcept>
      connection_stats_received;
};
//------------------------------------------------------------------------------
auto make_statistics_consumer_impl(subscriber&, statistics_consumer_signals&)
  -> std::unique_ptr<statistics_consumer_intf>;
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
    void query_statistics(identifier_t node_id) noexcept {
        _impl->query_statistics(node_id);
    }

    /// @brief Broadcasts network statistics query to all message bus nodes.
    /// @see query_statistics
    void discover_statistics() noexcept {
        query_statistics(broadcast_endpoint_id());
    }

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();
        _impl->add_methods();
    }

private:
    const std::unique_ptr<statistics_consumer_intf> _impl{
      make_statistics_consumer_impl(*this, *this)};
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


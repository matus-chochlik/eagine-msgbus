/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module eagine.msgbus.services;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.msgbus.core;
import std;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
class statistics_consumer_impl : public statistics_consumer_intf {

public:
    statistics_consumer_impl(
      subscriber& sub,
      statistics_consumer_signals& sigs) noexcept
      : base{sub}
      , signals{sigs} {}

    void add_methods() noexcept final {
        base.add_method(
          this,
          msgbus_map<"statsRutr", &statistics_consumer_impl::_handle_router>{});
        base.add_method(
          this,
          msgbus_map<"statsBrdg", &statistics_consumer_impl::_handle_bridge>{});
        base.add_method(
          this,
          msgbus_map<"statsEndpt", &statistics_consumer_impl::_handle_endpoint>{});
    }

    void query_statistics(identifier_t node_id) noexcept final {
        message_view message{};
        message.set_target_id(node_id);
        const auto msg_id{msgbus_id{"statsQuery"}};
        base.bus_node().post(msg_id, message);
    }

private:
    auto _handle_router(
      const message_context& msg_ctx,
      const stored_message& message) noexcept -> bool {
        router_statistics stats{};
        if(default_deserialize(stats, message.content())) {
            signals.router_stats_received(
              result_context{msg_ctx, message}, stats);
        }
        return true;
    }

    auto _handle_bridge(
      const message_context& msg_ctx,
      const stored_message& message) noexcept -> bool {
        bridge_statistics stats{};
        if(default_deserialize(stats, message.content())) {
            signals.bridge_stats_received(
              result_context{msg_ctx, message}, stats);
        }
        return true;
    }

    auto _handle_endpoint(
      const message_context& msg_ctx,
      const stored_message& message) noexcept -> bool {
        endpoint_statistics stats{};
        if(default_deserialize(stats, message.content())) {
            signals.endpoint_stats_received(
              result_context{msg_ctx, message}, stats);
        }
        return true;
    }

    auto _handle_connection(
      const message_context& msg_ctx,
      const stored_message& message) noexcept -> bool {
        connection_statistics stats{};
        if(default_deserialize(stats, message.content())) {
            signals.connection_stats_received(
              result_context{msg_ctx, message}, stats);
        }
        return true;
    }

    subscriber& base;
    statistics_consumer_signals& signals;
};
//------------------------------------------------------------------------------
auto make_statistics_consumer_impl(
  subscriber& base,
  statistics_consumer_signals& sigs)
  -> std::unique_ptr<statistics_consumer_intf> {
    return std::make_unique<statistics_consumer_impl>(base, sigs);
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

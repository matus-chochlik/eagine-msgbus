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
import eagine.msgbus.core;
import std;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
class network_topology_impl : public network_topology_intf {
    using This = network_topology_impl;

public:
    network_topology_impl(subscriber&, network_topology_signals& sigs) noexcept
      : signals{sigs} {}

    network_topology_signals& signals;

    void add_methods(subscriber& base) noexcept final {
        base.add_method(
          this, msgbus_map<"topoRutrCn", &This::_handle_router>{});
        base.add_method(
          this, msgbus_map<"topoBrdgCn", &This::_handle_bridge>{});
        base.add_method(
          this, msgbus_map<"topoEndpt", &This::_handle_endpoint>{});
        base.add_method(
          this, msgbus_map<"byeByeRutr", &This::_handle_router_bye>{});
        base.add_method(
          this, msgbus_map<"byeByeBrdg", &This::_handle_bridge_bye>{});
        base.add_method(
          this, msgbus_map<"byeByeEndp", &This::_handle_endpoint_bye>{});
    }

    void query_topology(endpoint& bus, const identifier_t node_id) noexcept
      final {
        message_view message{};
        message.set_target_id(node_id);
        const auto msg_id{msgbus_id{"topoQuery"}};
        bus.post(msg_id, message);
    }

private:
    auto _handle_router(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        router_topology_info info{};
        if(default_deserialize(info, message.content())) [[likely]] {
            signals.router_appeared(info);
        }
        return true;
    }

    auto _handle_bridge(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        bridge_topology_info info{};
        if(default_deserialize(info, message.content())) [[likely]] {
            signals.bridge_appeared(info);
        }
        return true;
    }

    auto _handle_endpoint(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        endpoint_topology_info info{};
        if(default_deserialize(info, message.content())) [[likely]] {
            signals.endpoint_appeared(info);
        }
        return true;
    }

    auto _handle_router_bye(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        signals.router_disappeared(message.source_id);
        return true;
    }

    auto _handle_bridge_bye(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        signals.bridge_disappeared(message.source_id);
        return true;
    }

    auto _handle_endpoint_bye(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        signals.endpoint_disappeared(message.source_id);
        return true;
    }
};
//------------------------------------------------------------------------------
auto make_network_topology_impl(subscriber& base, network_topology_signals& sigs)
  -> std::unique_ptr<network_topology_intf> {
    return std::make_unique<network_topology_impl>(base, sigs);
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


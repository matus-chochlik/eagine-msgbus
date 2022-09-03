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
import eagine.core.utility;
import eagine.msgbus.core;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
class subscriber_discovery_impl : public subscriber_discovery_intf {
public:
    subscriber_discovery_impl(
      subscriber& sub,
      subscriber_discovery_signals& sigs) noexcept
      : base{sub}
      , signals{sigs} {}

    void add_methods() noexcept final {
        base.add_method(
          this,
          msgbus_map<"stillAlive", &subscriber_discovery_impl::_handle_alive>{});
        base.add_method(
          this,
          msgbus_map<
            "subscribTo",
            &subscriber_discovery_impl::_handle_subscribed>{});
        base.add_method(
          this,
          msgbus_map<
            "unsubFrom",
            &subscriber_discovery_impl::_handle_unsubscribed>{});
        base.add_method(
          this,
          msgbus_map<
            "notSubTo",
            &subscriber_discovery_impl::_handle_not_subscribed>{});
    }

private:
    auto _handle_alive(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        subscriber_info info{};
        info.endpoint_id = message.source_id;
        info.instance_id = message.sequence_no;
        info.hop_count = message.hop_count;
        signals.reported_alive(info);
        return true;
    }

    auto _handle_subscribed(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        message_id sub_msg_id{};
        if(default_deserialize_message_type(sub_msg_id, message.content())) {
            subscriber_info info{};
            info.endpoint_id = message.source_id;
            info.instance_id = message.sequence_no;
            info.hop_count = message.hop_count;
            signals.subscribed(info, sub_msg_id);
        }
        return true;
    }

    auto _handle_unsubscribed(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        message_id sub_msg_id{};
        if(default_deserialize_message_type(sub_msg_id, message.content())) {
            subscriber_info info{};
            info.endpoint_id = message.source_id;
            info.instance_id = message.sequence_no;
            info.hop_count = message.hop_count;
            signals.unsubscribed(info, sub_msg_id);
        }
        return true;
    }

    auto _handle_not_subscribed(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        message_id sub_msg_id{};
        if(default_deserialize_message_type(sub_msg_id, message.content())) {
            subscriber_info info{};
            info.endpoint_id = message.source_id;
            info.instance_id = message.sequence_no;
            info.hop_count = message.hop_count;
            signals.not_subscribed(info, sub_msg_id);
        }
        return true;
    }

    subscriber& base;
    subscriber_discovery_signals& signals;
};
//------------------------------------------------------------------------------
auto make_subscriber_discovery_impl(
  subscriber& base,
  subscriber_discovery_signals& sigs)
  -> std::unique_ptr<subscriber_discovery_intf> {
    return std::make_unique<subscriber_discovery_impl>(base, sigs);
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

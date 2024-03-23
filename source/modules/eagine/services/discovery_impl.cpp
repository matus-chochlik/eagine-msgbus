/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
module eagine.msgbus.services;

import std;
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

    void add_methods() noexcept final;

    auto get_subscriber_info(const stored_message& message) noexcept
      -> subscriber_info;

    auto decode_subscriber_alive(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<subscriber_alive> final;

    auto decode_subscriber_subscribed(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<subscriber_subscribed> final;

    auto decode_subscriber_unsubscribed(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<subscriber_unsubscribed> final;

    auto decode_subscriber_not_subscribed(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<subscriber_not_subscribed> final;

private:
    auto _handle_alive(
      const message_context& msg_ctx,
      const stored_message& message) noexcept -> bool;

    auto _handle_subscribed(
      const message_context& msg_ctx,
      const stored_message& message) noexcept -> bool;

    auto _handle_unsubscribed(
      const message_context& msg_ctx,
      const stored_message& message) noexcept -> bool;

    auto _handle_not_subscribed(
      const message_context& msg_ctx,
      const stored_message& message) noexcept -> bool;

    subscriber& base;
    subscriber_discovery_signals& signals;
};
//------------------------------------------------------------------------------
auto subscriber_discovery_impl::_handle_alive(
  const message_context& msg_ctx,
  const stored_message& message) noexcept -> bool {
    signals.reported_alive(
      result_context{msg_ctx, message},
      subscriber_alive{.source = get_subscriber_info(message)});
    return true;
}
//------------------------------------------------------------------------------
auto subscriber_discovery_impl::_handle_subscribed(
  const message_context& msg_ctx,
  const stored_message& message) noexcept -> bool {
    message_id sub_msg_id{};
    if(default_deserialize_message_type(sub_msg_id, message.content())) {
        signals.subscribed(
          result_context{msg_ctx, message},
          subscriber_subscribed{
            .source = get_subscriber_info(message),
            .message_type = sub_msg_id});
    }
    return true;
}
//------------------------------------------------------------------------------
auto subscriber_discovery_impl::_handle_unsubscribed(
  const message_context& msg_ctx,
  const stored_message& message) noexcept -> bool {
    message_id sub_msg_id{};
    if(default_deserialize_message_type(sub_msg_id, message.content())) {
        signals.unsubscribed(
          result_context{msg_ctx, message},
          subscriber_unsubscribed{
            .source = get_subscriber_info(message),
            .message_type = sub_msg_id});
    }
    return true;
}
//------------------------------------------------------------------------------
auto subscriber_discovery_impl::_handle_not_subscribed(
  const message_context& msg_ctx,
  const stored_message& message) noexcept -> bool {
    message_id sub_msg_id{};
    if(default_deserialize_message_type(sub_msg_id, message.content())) {
        signals.not_subscribed(
          result_context{msg_ctx, message},
          subscriber_not_subscribed{
            .source = get_subscriber_info(message),
            .message_type = sub_msg_id});
    }
    return true;
}
//------------------------------------------------------------------------------
void subscriber_discovery_impl::add_methods() noexcept {
    base.add_method(
      this,
      msgbus_map<"stillAlive", &subscriber_discovery_impl::_handle_alive>{});
    base.add_method(
      this,
      msgbus_map<"subscribTo", &subscriber_discovery_impl::_handle_subscribed>{});
    base.add_method(
      this,
      msgbus_map<"unsubFrom", &subscriber_discovery_impl::_handle_unsubscribed>{});
    base.add_method(
      this,
      msgbus_map<
        "notSubTo",
        &subscriber_discovery_impl::_handle_not_subscribed>{});
}
//------------------------------------------------------------------------------
auto subscriber_discovery_impl::get_subscriber_info(
  const stored_message& message) noexcept -> subscriber_info {
    subscriber_info result{};
    result.endpoint_id = message.source_id;
    result.instance_id = message.sequence_no;
    result.hop_count = message.hop_count;
    return result;
}
//------------------------------------------------------------------------------
auto subscriber_discovery_impl::decode_subscriber_alive(
  const message_context& msg_ctx,
  const stored_message& message) noexcept -> std::optional<subscriber_alive> {
    if(msg_ctx.is_special_message("stillAlive")) {
        return {subscriber_alive{.source = get_subscriber_info(message)}};
    }
    return {};
}
//------------------------------------------------------------------------------
auto subscriber_discovery_impl::decode_subscriber_subscribed(
  const message_context& msg_ctx,
  const stored_message& message) noexcept
  -> std::optional<subscriber_subscribed> {
    if(msg_ctx.is_special_message("subscribTo")) {
        message_id sub_msg_id{};
        if(default_deserialize_message_type(sub_msg_id, message.content())) {
            return {subscriber_subscribed{
              .source = get_subscriber_info(message),
              .message_type = sub_msg_id}};
        }
    }
    return {};
}
//------------------------------------------------------------------------------
auto subscriber_discovery_impl::decode_subscriber_unsubscribed(
  const message_context& msg_ctx,
  const stored_message& message) noexcept
  -> std::optional<subscriber_unsubscribed> {
    if(msg_ctx.is_special_message("unsubFrom")) {
        message_id sub_msg_id{};
        if(default_deserialize_message_type(sub_msg_id, message.content())) {
            return {subscriber_unsubscribed{
              .source = get_subscriber_info(message),
              .message_type = sub_msg_id}};
        }
    }
    return {};
}
//------------------------------------------------------------------------------
auto subscriber_discovery_impl::decode_subscriber_not_subscribed(
  const message_context& msg_ctx,
  const stored_message& message) noexcept
  -> std::optional<subscriber_not_subscribed> {
    if(msg_ctx.is_special_message("notSubTo")) {
        message_id sub_msg_id{};
        if(default_deserialize_message_type(sub_msg_id, message.content())) {
            return {subscriber_not_subscribed{
              .source = get_subscriber_info(message),
              .message_type = sub_msg_id}};
        }
    }
    return {};
}
//------------------------------------------------------------------------------
auto make_subscriber_discovery_impl(
  subscriber& base,
  subscriber_discovery_signals& sigs)
  -> unique_holder<subscriber_discovery_intf> {
    return {hold<subscriber_discovery_impl>, base, sigs};
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

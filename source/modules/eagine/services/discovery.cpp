/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.services:discovery;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.utility;
import eagine.msgbus.core;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Structure containing basic information about a message bus endpoint.
export struct subscriber_info {
    /// @brief The endpoint id.
    identifier_t endpoint_id{0U};
    /// @brief The endpoint's instance (process) id.
    process_instance_id_t instance_id{0U};

    /// @brief The type storing distance in number of hops to the endpoint.
    using hop_count_t = std::int8_t;
    /// @brief The distance in number of bus node hops to the endpoint.
    /// @see max_hops
    hop_count_t hop_count{0};

    /// @brief Returns the maximum possible value for hop_count.
    /// @see hop_count
    static constexpr auto max_hops() noexcept -> hop_count_t {
        return std::numeric_limits<hop_count_t>::max();
    }
};
//------------------------------------------------------------------------------
export struct subscriber_alive {
    subscriber_info source{};
};

export struct subscriber_subscribed {
    subscriber_info source{};
    message_id message_type{};
};

export struct subscriber_unsubscribed {
    subscriber_info source{};
    message_id message_type{};
};

export struct subscriber_not_subscribed {
    subscriber_info source{};
    message_id message_type{};
};
//------------------------------------------------------------------------------
/// @brief Collection of signals emitted by the subscriber discovery service.
/// @ingroup msgbus
/// @see subscriber_discovery
/// @see subscriber_info
export struct subscriber_discovery_signals {
    /// @brief Triggered on receipt of notification that an endpoint is alive.
    signal<void(const result_context&, const subscriber_alive&) noexcept>
      reported_alive;

    /// @brief Triggered on receipt of info that endpoint subscribes to message.
    signal<void(const result_context&, const subscriber_subscribed&) noexcept>
      subscribed;

    /// @brief Triggered on receipt of info that endpoint unsubscribes from message.
    signal<void(const result_context&, const subscriber_unsubscribed&) noexcept>
      unsubscribed;

    /// @brief Triggered on receipt of info that endpoint doesn't handle message type.
    signal<void(const result_context&, const subscriber_not_subscribed&) noexcept>
      not_subscribed;
};
//------------------------------------------------------------------------------
struct subscriber_discovery_intf : interface<subscriber_discovery_intf> {
    virtual void add_methods() noexcept = 0;

    virtual auto decode_subscriber_alive(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<subscriber_alive> = 0;

    virtual auto decode_subscriber_subscribed(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<subscriber_subscribed> = 0;

    virtual auto decode_subscriber_unsubscribed(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<subscriber_unsubscribed> = 0;

    virtual auto decode_subscriber_not_subscribed(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<subscriber_not_subscribed> = 0;
};
//------------------------------------------------------------------------------
auto make_subscriber_discovery_impl(
  subscriber& base,
  subscriber_discovery_signals&) -> std::unique_ptr<subscriber_discovery_intf>;
//------------------------------------------------------------------------------
/// @brief Service discovering information about endpoint status and subscriptions.
/// @ingroup msgbus
/// @see service_composition
/// @see subscriber_info
export template <typename Base = subscriber>
class subscriber_discovery
  : public Base
  , public subscriber_discovery_signals {
public:
    auto decode_subscriber_alive(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<subscriber_alive> {
        return _impl->decode_subscriber_alive(msg_ctx, message);
    }

    auto decode_subscriber_subscribed(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<subscriber_subscribed> {
        return _impl->decode_subscriber_subscribed(msg_ctx, message);
    }

    auto decode_subscriber_unsubscribed(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<subscriber_unsubscribed> {
        return _impl->decode_subscriber_unsubscribed(msg_ctx, message);
    }

    auto decode_subscriber_not_subscribed(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<subscriber_not_subscribed> {
        return _impl->decode_subscriber_not_subscribed(msg_ctx, message);
    }

    auto decode(const message_context& msg_ctx, const stored_message& message) {
        return this->decode_chain(
          msg_ctx,
          message,
          *static_cast<Base*>(this),
          *this,
          &subscriber_discovery::decode_subscriber_alive,
          &subscriber_discovery::decode_subscriber_subscribed,
          &subscriber_discovery::decode_subscriber_unsubscribed,
          &subscriber_discovery::decode_subscriber_not_subscribed);
    }

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();
        _impl->add_methods();
    }

private:
    const std::unique_ptr<subscriber_discovery_intf> _impl{
      make_subscriber_discovery_impl(*this, *this)};
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


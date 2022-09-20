/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.services:discovery;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.utility;
import eagine.msgbus.core;
import <limits>;

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
/// @brief Collection of signals emitted by the subscriber discovery service.
/// @ingroup msgbus
/// @see subscriber_discovery
/// @see subscriber_info
export struct subscriber_discovery_signals {
    /// @brief Triggered on receipt of notification that an endpoint is alive.
    signal<void(const subscriber_info&) noexcept> reported_alive;

    /// @brief Triggered on receipt of info that endpoint subscribes to message.
    signal<void(const subscriber_info&, const message_id) noexcept> subscribed;

    /// @brief Triggered on receipt of info that endpoint unsubscribes from message.
    signal<void(const subscriber_info&, const message_id) noexcept> unsubscribed;

    /// @brief Triggered on receipt of info that endpoint doesn't handle message type.
    signal<void(const subscriber_info&, const message_id) noexcept>
      not_subscribed;
};
//------------------------------------------------------------------------------
struct subscriber_discovery_intf : interface<subscriber_discovery_intf> {
    virtual void add_methods() noexcept = 0;
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


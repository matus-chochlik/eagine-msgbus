/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.services:ping_pong;

import eagine.core.types;
import eagine.core.debug;
import eagine.core.memory;
import eagine.core.utility;
import eagine.msgbus.core;
import <chrono>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Service responding to pings from the pinger counterpart.
/// @ingroup msgbus
/// @see service_composition
/// @see pinger
export template <typename Base = subscriber>
class pingable : public Base {
    using This = pingable;

public:
    /// @brief Decides if a ping request should be responded.
    virtual auto respond_to_ping(
      [[maybe_unused]] const identifier_t pinger_id,
      const message_sequence_t,
      const verification_bits) noexcept -> bool {
        return true;
    }

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();
        Base::add_method(this, msgbus_map<"ping", &This::_handle_ping>{});
    }

private:
    auto _handle_ping(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        if(respond_to_ping(
             message.source_id,
             message.sequence_no,
             this->verify_bits(message))) {
            this->bus_node().respond_to(message, msgbus_id{"pong"}, {});
        }
        return true;
    }
};
//------------------------------------------------------------------------------
struct pinger_intf : interface<pinger_intf> {
    virtual void add_methods() noexcept = 0;

    virtual void query_pingables() noexcept = 0;

    virtual void ping(
      const identifier_t pingable_id,
      const std::chrono::milliseconds max_time) noexcept = 0;

    virtual auto update() noexcept -> work_done = 0;

    virtual auto has_pending_pings() noexcept -> bool = 0;
};
//------------------------------------------------------------------------------
/// @brief Collection of signals emitted by the pinger service.
/// @ingroup msgbus
/// @see service_composition
/// @see pinger
export struct pinger_signals {

    /// @brief Triggered on receipt of ping response.
    /// @see ping
    /// @see ping_timeouted
    /// @see has_pending_pings
    signal<void(
      const identifier_t pingable_id,
      const message_sequence_t sequence_no,
      const std::chrono::microseconds age,
      const verification_bits) noexcept>
      ping_responded;

    /// @brief Triggered on timeout of ping response.
    /// @see ping
    /// @see ping_responded
    /// @see has_pending_pings
    signal<void(
      const identifier_t pingable_id,
      const message_sequence_t sequence_no,
      const std::chrono::microseconds age) noexcept>
      ping_timeouted;
};
//------------------------------------------------------------------------------
auto make_pinger_impl(subscriber&, pinger_signals&)
  -> std::unique_ptr<pinger_intf>;
//------------------------------------------------------------------------------
/// @brief Service sending to pings from the pingable counterparts.
/// @ingroup msgbus
/// @see service_composition
/// @see pingable
export template <typename Base = subscriber>
class pinger
  : public Base
  , public pinger_signals
  , protected std::chrono::steady_clock {

public:
    static constexpr auto ping_msg_id() noexcept {
        return msgbus_id{"ping"};
    }

    /// @brief Broadcasts a query searching for pingable message bus nodes.
    void query_pingables() noexcept {
        _impl->query_pingables();
    }

    /// @brief Sends a pings request and tracks it for the specified maximum time.
    /// @see ping_responded
    /// @see ping_timeouted
    /// @see has_pending_pings
    void ping(
      const identifier_t pingable_id,
      const std::chrono::milliseconds max_time) noexcept {
        _impl->ping(pingable_id, max_time);
    }

    /// @brief Sends a pings request and tracks it for a default time period.
    /// @see ping_responded
    /// @see ping_timeouted
    /// @see has_pending_pings
    void ping(const identifier_t pingable_id) noexcept {
        ping(
          pingable_id,
          adjusted_duration(
            std::chrono::milliseconds{5000}, memory_access_rate::low));
    }

    auto update() noexcept -> work_done {
        some_true something_done{Base::update()};
        something_done(_impl->update());

        return something_done;
    }

    /// @brief Indicates if there are yet unresponded pending ping requests.
    /// @see ping_responded
    /// @see ping_timeouted
    auto has_pending_pings() const noexcept -> bool {
        return _impl->has_pending_pings();
    }

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();
        _impl->add_methods();
    }

private:
    const std::unique_ptr<pinger_intf> _impl{make_pinger_impl(*this, *this)};
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#ifndef EAGINE_MSGBUS_SERVICE_PING_PONG_HPP
#define EAGINE_MSGBUS_SERVICE_PING_PONG_HPP

#include "../serialize.hpp"
#include "../signal.hpp"
#include "../subscriber.hpp"
#include <eagine/bool_aggregate.hpp>
#include <chrono>
#include <vector>

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Service responding to pings from the pinger counterpart.
/// @ingroup msgbus
/// @see service_composition
/// @see pinger
template <typename Base = subscriber>
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
        Base::add_method(
          this, EAGINE_MSG_MAP(eagiMsgBus, ping, This, _handle_ping));
    }

private:
    auto _handle_ping(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        if(respond_to_ping(
             message.source_id,
             message.sequence_no,
             this->verify_bits(message))) {
            this->bus_node().respond_to(message, EAGINE_MSGBUS_ID(pong), {});
        }
        return true;
    }
};
//------------------------------------------------------------------------------
/// @brief Service sending to pings from the pingable counterparts.
/// @ingroup msgbus
/// @see service_composition
/// @see pingable
template <typename Base = subscriber>
class pinger
  : public Base
  , protected std::chrono::steady_clock {

    using This = pinger;

    std::vector<std::tuple<identifier_t, message_sequence_t, timeout>>
      _pending{};

public:
    /// @brief Returns the ping message type id.
    static constexpr auto ping_msg_id() noexcept {
        return EAGINE_MSGBUS_ID(ping);
    }

    /// @brief Broadcasts a query searching for pingable message bus nodes.
    void query_pingables() noexcept {
        this->bus_node().query_subscribers_of(ping_msg_id());
    }

    /// @brief Sends a pings request and tracks it for the specified maximum time.
    /// @see ping_responded
    /// @see ping_timeouted
    /// @see has_pending_pings
    void ping(
      const identifier_t pingable_id,
      const std::chrono::milliseconds max_time) noexcept {
        message_view message{};
        auto msg_id{EAGINE_MSGBUS_ID(ping)};
        message.set_target_id(pingable_id);
        message.set_priority(message_priority::low);
        this->bus_node().set_next_sequence_id(msg_id, message);
        this->bus_node().post(msg_id, message);
        _pending.emplace_back(message.target_id, message.sequence_no, max_time);
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
        some_true something_done{};
        something_done(Base::update());

        something_done(
          std::erase_if(_pending, [this](auto& entry) {
              auto& [pingable_id, sequence_no, ping_time] = entry;
              if(ping_time.is_expired()) {
                  ping_timeouted(
                    pingable_id,
                    sequence_no,
                    std::chrono::duration_cast<std::chrono::microseconds>(
                      ping_time.elapsed_time()));
                  return true;
              }
              return false;
          }) > 0);
        return something_done;
    }

    /// @brief Indicates if there are yet unresponded pending ping requests.
    /// @see ping_responded
    /// @see ping_timeouted
    auto has_pending_pings() const noexcept -> bool {
        return !_pending.empty();
    }

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

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();
        Base::add_method(
          this, EAGINE_MSG_MAP(eagiMsgBus, pong, This, _handle_pong));
    }

private:
    auto _handle_pong(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        std::erase_if(_pending, [this, &message](auto& entry) {
            auto& [pingable_id, sequence_no, ping_time] = entry;
            const bool is_response = (message.source_id == pingable_id) &&
                                     (message.sequence_no == sequence_no);
            if(is_response) {
                ping_responded(
                  message.source_id,
                  message.sequence_no,
                  std::chrono::duration_cast<std::chrono::microseconds>(
                    ping_time.elapsed_time()),
                  this->verify_bits(message));
                return true;
            }
            return false;
        });
        return true;
    }
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

#endif // EAGINE_MSGBUS_SERVICE_PING_PONG_HPP

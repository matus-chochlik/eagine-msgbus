/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

export module eagine.msgbus.services:shutdown;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.utility;
import eagine.msgbus.core;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
// TODO replace with utc clock when available
export using shutdown_service_clock = std::chrono::system_clock;
export using shutdown_service_duration =
  std::chrono::duration<std::int64_t, std::milli>;
//------------------------------------------------------------------------------
/// @brief
/// @ingroup msgbus
/// @see shutdown_target_signals
export struct shutdown_request {
    /// @brief Id of the endpoint that sent the request.
    endpoint_id_t source_id;
    /// @brief The age of the request.
    std::chrono::milliseconds age;
    /// @brief Bitfield indicating what part of the message could be verified.
    verification_bits verified;
};
//------------------------------------------------------------------------------
/// @brief Collection of signals emitted by the shutdown target service.
/// @ingroup msgbus
/// @see shutdown_target
/// @see shutdown_invoker
export struct shutdown_target_signals {
    /// @brief Triggered when a shutdown request is received.
    signal<void(const result_context&, const shutdown_request&) noexcept>
      shutdown_requested;
};
//------------------------------------------------------------------------------
struct shutdown_target_intf : interface<shutdown_target_intf> {
    virtual void add_methods() noexcept = 0;

    virtual auto decode_shutdown_request(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<shutdown_request> = 0;
};
//------------------------------------------------------------------------------
auto make_shutdown_target_impl(subscriber&, shutdown_target_signals&)
  -> unique_holder<shutdown_target_intf>;
//------------------------------------------------------------------------------
/// @brief Service allowing an endpoint to be shut down over the message bus.
/// @ingroup msgbus
/// @see service_composition
/// @see shutdown_invoker
export template <typename Base = subscriber>
class shutdown_target
  : public Base
  , public shutdown_target_signals {
public:
    auto decode_shutdown_request(
      const message_context& msg_ctx,
      const stored_message& message) noexcept
      -> std::optional<shutdown_request> {
        return _impl->decode_shutdown_request(msg_ctx, message);
    }

    auto decode(const message_context& msg_ctx, const stored_message& message) {
        return this->decode_chain(
          msg_ctx,
          message,
          *static_cast<Base*>(this),
          *this,
          &shutdown_target::decode_shutdown_request);
    }

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();
        _impl->add_methods();
    }

private:
    const unique_holder<shutdown_target_intf> _impl{
      make_shutdown_target_impl(*this, *this)};
};
//------------------------------------------------------------------------------
/// @brief Service allowing to shut down other endpoints over the message bus.
/// @ingroup msgbus
/// @see service_composition
/// @see shutdown_target
export template <typename Base = subscriber>
class shutdown_invoker
  : public Base
  , protected shutdown_service_clock {

    using This = shutdown_invoker;

public:
    /// @brief Sends shutdown request to the specified target endpoint.
    void shutdown_one(const endpoint_id_t target_id) noexcept {
        std::array<byte, 32> temp{};
        const auto ts{this->now()};
        const auto ticks{std::chrono::duration_cast<shutdown_service_duration>(
          ts.time_since_epoch())};
        const auto count{ticks.count()};
        auto serialized{default_serialize(count, cover(temp))};
        assert(serialized);

        const message_id msg_id{"Shutdown", "shutdown"};
        message_view message{*serialized};
        message.set_target_id(target_id);
        if(not this->bus_node().post_signed(msg_id, message)) {
            this->bus_node().post(msg_id, message);
        }
    }

protected:
    using Base::Base;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


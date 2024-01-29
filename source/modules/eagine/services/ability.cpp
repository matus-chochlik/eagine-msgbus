/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

export module eagine.msgbus.services:ability;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.serialization;
import eagine.core.utility;
import eagine.core.valid_if;
import eagine.msgbus.core;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
export class ability_query {
public:
    constexpr ability_query() noexcept = default;

    constexpr ability_query(message_id msg_id) noexcept
      : _msg_id{msg_id} {}

    constexpr auto queried_message_type() const noexcept -> message_id {
        return _msg_id;
    }

private:
    message_id _msg_id;
};
//------------------------------------------------------------------------------
/// @brief Service providing information about message types handled by endpoint.
/// @ingroup msgbus
/// @see service_composition
/// @see ability_tester
export template <typename Base = subscriber>
class ability_provider : public Base {
    using This = ability_provider;

public:
    /// @brief Indicates if the given message type is handled by the endpoint.
    virtual auto can_handle(const message_id) -> bool = 0;

    auto can_handle(const ability_query& query) noexcept -> bool {
        return can_handle(query.queried_message_type());
    }

    auto do_decode_ability_query(
      const message_context& msg_ctx,
      const stored_message& message) -> std::optional<ability_query> {
        return default_deserialized_message_type(message.content())
          .construct<ability_query>()
          .to_optional();
    }

    auto decode_ability_query(
      const message_context& msg_ctx,
      const stored_message& message) -> std::optional<ability_query> {
        if(msg_ctx.msg_id().is("Ability", "query")) {
            return do_decode_ability_query(msg_ctx, message);
        }
        return {};
    }

    auto decode(const message_context& msg_ctx, const stored_message& message) {
        return this->decode_chain(
          msg_ctx,
          message,
          *static_cast<Base*>(this),
          *this,
          &ability_provider::decode_ability_query);
    }

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();
        Base::add_method(
          this, message_map<"Ability", "query", &This::_handle_query>{});
    }

private:
    auto _handle_query(
      const message_context& msg_ctx,
      const stored_message& message) noexcept -> bool {
        decode_ability_query(msg_ctx, message).and_then([&](const auto& query) {
            if(can_handle(query)) {
                msg_ctx.bus_node().respond_to(
                  message,
                  message_id{"Ability", "response"},
                  {message.content()});
            }
        });
        return true;
    }
};
//------------------------------------------------------------------------------
export class ability_info {
public:
    constexpr ability_info() noexcept = default;

    constexpr ability_info(message_id msg_id, endpoint_id_t endpoint_id) noexcept
      : _msg_id{msg_id}
      , _endpoint_id{endpoint_id} {}

    constexpr auto supported_message_type() const noexcept -> message_id {
        return _msg_id;
    }

private:
    message_id _msg_id;
    endpoint_id_t _endpoint_id{};
};
//------------------------------------------------------------------------------
/// @brief Service consuming information about message types handled by endpoint.
/// @ingroup msgbus
/// @see service_composition
/// @see ability_provider
export template <typename Base = subscriber>
class ability_tester : public Base {

    using This = ability_tester;

public:
    /// @brief Sends a query to endpoints if they handle the specified message type.
    /// @see handler_found
    void find_handler(const message_id msg_id) noexcept {
        std::array<byte, 32> temp{};
        auto serialized{default_serialize(msg_id, cover(temp))};
        assert(serialized);

        this->bus_node().broadcast(message_id{"Ability", "query"}, *serialized);
    }

    auto do_decode_ability_info(
      const message_context& msg_ctx,
      const stored_message& message) -> std::optional<ability_info> {
        return default_deserialized_message_type(message.content())
          .construct<ability_info>(message.source_id)
          .to_optional();
    }

    auto decode_ability_info(
      const message_context& msg_ctx,
      const stored_message& message) -> std::optional<ability_info> {
        if(msg_ctx.msg_id().is("Ability", "response")) {
            return do_decode_ability_info(msg_ctx, message);
        }
        return {};
    }

    auto decode(const message_context& msg_ctx, const stored_message& message) {
        return this->decode_chain(
          msg_ctx,
          message,
          *static_cast<Base*>(this),
          *this,
          &ability_tester::decode_ability_info);
    }

    /// @brief Triggered on receipt of response about message handling by endpoint.
    /// @see find_handler
    signal<void(const result_context&, const ability_info&) noexcept>
      handler_found;

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();
        Base::add_method(
          this, message_map<"Ability", "response", &This::_handle_response>{});
    }

private:
    auto _handle_response(
      const message_context& msg_ctx,
      const stored_message& message) noexcept -> bool {
        message_id msg_id{};
        if(default_deserialize_message_type(msg_id, message.content())) {
            handler_found(
              result_context{msg_ctx, message},
              ability_info{msg_id, message.source_id});
        }
        return true;
    }
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


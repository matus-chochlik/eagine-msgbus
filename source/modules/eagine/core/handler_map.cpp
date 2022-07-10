/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.core:handler_map;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Represents a mapping from a message type id to member function constant.
/// @ingroup msgbus
/// @see message_id
/// @see static_message_handler_map
/// @see member_function_constant
/// @see static_subscriber
/// @see subscriber
export template <typename MemFuncConst>
struct message_handler_map {
    message_id _msg_id;

    /// @brief Construction from message type id.
    constexpr message_handler_map(message_id msgid) noexcept
      : _msg_id{std::move(msgid)} {}

    /// @brief Returns the message type id.
    constexpr auto msg_id() const noexcept -> message_id {
        return _msg_id;
    }

    /// @brief Returns the member function constant.
    static constexpr auto method() noexcept -> MemFuncConst {
        return {};
    }
};
//------------------------------------------------------------------------------
/// @brief Represents a mapping from a message type id to member function constant.
/// @ingroup msgbus
/// @see message_id
/// @see message_handler_map
/// @see member_function_constant
/// @see static_subscriber
/// @see subscriber
export template <typename MessageId, typename MemFuncConst>
struct static_message_handler_map {

    /// @brief Returns the message type id.
    static constexpr auto msg_id() noexcept -> MessageId {
        return {};
    }

    /// @brief Returns the member function constant.
    static constexpr auto method() noexcept -> MemFuncConst {
        return {};
    }
};
//------------------------------------------------------------------------------
/// @brief Constructs an instance of static message handler map.
/// @ingroup msgbus
/// @see eagine::msgbus::static_message_handler_map
/// @see eagine::msgbus::static_subscriber
/// @see eagine::msgbus::subscriber
export template <identifier_t ClassId, identifier_t MethodId, auto MemFuncPtr>
using message_map = static_message_handler_map<
  static_message_id<ClassId, MethodId>,
  member_function_constant_t<MemFuncPtr>>;

export template <identifier_t MethodId, auto MemFuncPtr>
using mgsbus_map = message_map<id_v("eagiMsgBus"), MethodId, MemFuncPtr>;
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


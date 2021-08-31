/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#ifndef EAGINE_MSGBUS_VERIFICATION_HPP
#define EAGINE_MSGBUS_VERIFICATION_HPP

#include <eagine/bitfield.hpp>
#include <eagine/reflect/map_enumerators.hpp>
#include <cstdint>

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Bus message verification bits enumeration,
/// @ingroup msgbus
/// @see verification_bits
enum class verification_bit : std::uint8_t {
    /// @brief The source has been verified.
    source_id = 1U << 0U,
    /// @brief The source certificate has been verified
    source_certificate = 1U << 1U,
    /// @brief The source private key has been verified.
    source_private_key = 1U << 2U,
    /// @brief The message type id has been verified.
    message_id = 1U << 3U,
    /// @brief The message content has been verified.
    message_content = 1U << 4U
};
//------------------------------------------------------------------------------
#if !EAGINE_CXX_REFLECTION
template <typename Selector>
constexpr auto enumerator_mapping(
  const type_identity<verification_bit>,
  const Selector) noexcept {
    return enumerator_map_type<verification_bit, 5>{
      {{"source_id", verification_bit::source_id},
       {"source_certificate", verification_bit::source_certificate},
       {"source_private_key", verification_bit::source_private_key},
       {"message_id", verification_bit::message_id},
       {"message_content", verification_bit::message_content}}};
}
#endif
//------------------------------------------------------------------------------
/// @brief Alias for a bus message verification bitfield.
/// @ingroup msgbus
using verification_bits = bitfield<verification_bit>;
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

#endif // EAGINE_MSGBUS_VERIFICATION_HPP

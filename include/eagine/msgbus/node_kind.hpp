/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#ifndef EAGINE_MSGBUS_NODE_KIND_HPP
#define EAGINE_MSGBUS_NODE_KIND_HPP

#include <eagine/reflect/map_enumerators.hpp>

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Message bus node kind enumeration.
/// @ingroup msgbus
enum class node_kind : std::uint8_t {
    /// @brief Unknown node kind.
    unknown,
    /// @brief Message bus client endpoint.
    endpoint,
    /// @brief Message bus bridge.
    bridge,
    /// @brief Message bus router.
    router
};
//------------------------------------------------------------------------------
template <typename Selector>
constexpr auto enumerator_mapping(
  const type_identity<node_kind>,
  const Selector) noexcept {
    return enumerator_map_type<node_kind, 4>{
      {{"unknown", node_kind::unknown},
       {"endpoint", node_kind::endpoint},
       {"bridge", node_kind::bridge},
       {"router", node_kind::router}}};
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

#endif // EAGINE_MSGBUS_NODE_KIND_HPP

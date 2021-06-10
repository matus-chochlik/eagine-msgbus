/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#ifndef EAGINE_MESSAGE_BUS_TYPES_HPP
#define EAGINE_MESSAGE_BUS_TYPES_HPP

#include "connection_kind.hpp"
#include <eagine/identifier_t.hpp>
#include <eagine/reflect/map_data_members.hpp>
#include <eagine/types.hpp>
#include <tuple>

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Alias for message sequence number type.
/// @ingroup msgbus
using message_sequence_t = std::uint32_t;
//------------------------------------------------------------------------------
/// @brief Structure holding part of router connection topology information.
/// @ingroup msgbus
struct router_topology_info {
    /// @brief The router message bus id.
    identifier_t router_id{0};

    /// @brief The remote node message bus id.
    identifier_t remote_id{0};

    /// @brief The router process instance id.
    process_instance_id_t instance_id{0U};

    /// @brief The connection kind.
    connection_kind connect_kind{0U};
};

template <typename Selector>
constexpr auto
data_member_mapping(type_identity<router_topology_info>, Selector) noexcept {
    using S = router_topology_info;
    return make_data_member_mapping<
      S,
      identifier_t,
      identifier_t,
      process_instance_id_t,
      connection_kind>(
      {"router_id", &S::router_id},
      {"remote_id", &S::remote_id},
      {"instance_id", &S::instance_id},
      {"connect_kind", &S::connect_kind});
}
//------------------------------------------------------------------------------
/// @brief Structure holding router statistics information.
/// @ingroup msgbus
struct router_statistics {
    /// @brief Number of forwarded messages.
    std::int64_t forwarded_messages{0};

    /// @brief Number of dropped messages.
    std::int64_t dropped_messages{0};

    /// @brief Average message age in milliseconds
    std::int32_t message_age_us{0};

    /// @brief Number of forwarded messages per second.
    std::int32_t messages_per_second{0};

    /// @brief Uptime in seconds.
    std::int64_t uptime_seconds{0};
};

template <typename Selector>
constexpr auto
data_member_mapping(type_identity<router_statistics>, Selector) noexcept {
    using S = router_statistics;
    return make_data_member_mapping<
      S,
      std::int64_t,
      std::int64_t,
      std::int32_t,
      std::int32_t,
      std::int64_t>(
      {"forwarded_messages", &S::forwarded_messages},
      {"dropped_messages", &S::dropped_messages},
      {"message_age_us", &S::message_age_us},
      {"messages_per_second", &S::messages_per_second},
      {"uptime_seconds", &S::uptime_seconds});
}
//------------------------------------------------------------------------------
/// @brief Structure holding part of bridge connection topology information.
/// @ingroup msgbus
struct bridge_topology_info {
    /// @brief The bridge message bus id.
    identifier_t bridge_id{0};

    /// @brief The remote node message bus id.
    identifier_t opposite_id{0};

    /// @brief The bridge process instance id.
    process_instance_id_t instance_id{0U};
};

template <typename Selector>
constexpr auto
data_member_mapping(type_identity<bridge_topology_info>, Selector) noexcept {
    using S = bridge_topology_info;
    return make_data_member_mapping<
      S,
      identifier_t,
      identifier_t,
      process_instance_id_t>(
      {"bridge_id", &S::bridge_id},
      {"opposite_id", &S::opposite_id},
      {"instance_id", &S::instance_id});
}
//------------------------------------------------------------------------------
/// @brief Structure holding bridge statistics information.
/// @ingroup msgbus
struct bridge_statistics {
    /// @brief Number of forwarded messages.
    std::int64_t forwarded_messages{0};

    /// @brief Number of dropped messages.
    std::int64_t dropped_messages{0};

    /// @brief Average message age in milliseconds
    std::int32_t message_age_milliseconds{0};

    /// @brief Number of forwarded messages per second.
    std::int32_t messages_per_second{0};

    /// @brief Uptime in seconds.
    std::int64_t uptime_seconds{0};
};

template <typename Selector>
constexpr auto
data_member_mapping(type_identity<bridge_statistics>, Selector) noexcept {
    using S = bridge_statistics;
    return make_data_member_mapping<
      S,
      std::int64_t,
      std::int64_t,
      std::int32_t,
      std::int32_t,
      std::int64_t>(
      {"forwarded_messages", &S::forwarded_messages},
      {"dropped_messages", &S::dropped_messages},
      {"message_age_milliseconds", &S::message_age_milliseconds},
      {"messages_per_second", &S::messages_per_second},
      {"uptime_seconds", &S::uptime_seconds});
}
//------------------------------------------------------------------------------
/// @brief Structure holding part of endpoint connection topology information.
/// @ingroup msgbus
struct endpoint_topology_info {
    /// @brief The endpoint message bus id.
    identifier_t endpoint_id{0U};

    /// @brief The endpoint process instance id.
    process_instance_id_t instance_id{0U};
};

template <typename Selector>
constexpr auto
data_member_mapping(type_identity<endpoint_topology_info>, Selector) noexcept {
    using S = endpoint_topology_info;
    return make_data_member_mapping<S, identifier_t, process_instance_id_t>(
      {"endpoint_id", &S::endpoint_id}, {"instance_id", &S::instance_id});
}
//------------------------------------------------------------------------------
/// @brief Structure holding endpoint statistics information.
/// @ingroup msgbus
struct endpoint_statistics {
    /// @brief Number of sent messages.
    std::int64_t sent_messages{0};

    /// @brief Number of received messages.
    std::int64_t received_messages{0};

    /// @brief Number of dropped messages.
    std::int64_t dropped_messages{0};

    /// @brief Uptime in seconds.
    std::int64_t uptime_seconds{0};
};

template <typename Selector>
constexpr auto
data_member_mapping(type_identity<endpoint_statistics>, Selector) noexcept {
    using S = endpoint_statistics;
    return make_data_member_mapping<
      S,
      std::int64_t,
      std::int64_t,
      std::int64_t,
      std::int64_t>(
      {"sent_messages", &S::sent_messages},
      {"received_messages", &S::received_messages},
      {"dropped_messages", &S::dropped_messages},
      {"uptime_seconds", &S::uptime_seconds});
}
//------------------------------------------------------------------------------
/// @brief Message bus endpoint information.
/// @ingroup msgbus
struct endpoint_info {
    /// @brief User-readable display name of the endpoint.
    std::string display_name;

    /// @brief User-readable description of the endpoint.
    std::string description;

    /// @brief Indicates if the endpoint is a router control node.
    bool is_router_node{false};

    /// @brief Indicates if the endpoint is a bridge control node.
    bool is_bridge_node{false};

    auto tie() const noexcept {
        return std::tie(
          display_name, description, is_router_node, is_bridge_node);
    }

    friend auto
    operator!=(const endpoint_info& l, const endpoint_info& r) noexcept
      -> bool {
        return l.tie() != r.tie();
    }
};

template <typename Selector>
constexpr auto
data_member_mapping(type_identity<endpoint_info>, Selector) noexcept {
    using S = endpoint_info;
    return make_data_member_mapping<S, std::string, std::string, bool, bool>(
      {"display_name", &S::display_name},
      {"description", &S::description},
      {"is_router_node", &S::is_router_node},
      {"is_bridge_node", &S::is_bridge_node});
}
//------------------------------------------------------------------------------
/// @brief Structure holding message bus connection statistics.
/// @ingroup msgbus
struct connection_statistics {
    /// @brief The local node message bus id.
    identifier_t local_id{0};

    /// @brief The remote node message bus id.
    identifier_t remote_id{0};

    /// @brief Ratio (0.0 - 1.0) of how much of each message data block is used.
    float block_usage_ratio{-1.F};

    /// @brief Number of bytes per second transfered.
    float bytes_per_second{-1.F};
};

template <typename selector>
constexpr auto
data_member_mapping(type_identity<connection_statistics>, selector) noexcept {
    using s = connection_statistics;
    return make_data_member_mapping<s, identifier_t, identifier_t, float, float>(
      {"local_id", &s::local_id},
      {"remote_id", &s::remote_id},
      {"block_usage_ratio", &s::block_usage_ratio},
      {"bytes_per_second", &s::bytes_per_second});
}
//------------------------------------------------------------------------------
/// @brief Structure holding message bus data flow information.
/// @ingroup msgbus
struct message_flow_info {
    /// @brief The average age of message in seconds.
    std::int16_t avg_msg_age_ms{0};
};

template <typename selector>
constexpr auto
data_member_mapping(type_identity<message_flow_info>, selector) noexcept {
    using s = message_flow_info;
    return make_data_member_mapping<s, std::int16_t>(
      {"avg_msg_age_ms", &s::avg_msg_age_ms});
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

#endif // EAGINE_MESSAGE_BUS_TYPES_HPP

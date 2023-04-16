/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.core:types;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.string;
import eagine.core.reflection;
import eagine.core.identifier;

namespace eagine {
namespace msgbus {
//------------------------------------------------------------------------------
/// @brief Message bus node kind enumeration.
/// @ingroup msgbus
export enum class node_kind : std::uint8_t {
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
/// @brief Message bus connection kind bits enumeration.
/// @ingroup msgbus
/// @see connection_kinds
export enum class connection_kind : std::uint8_t {
    /// @brief Unknown connection kind.
    unknown = 0U,
    /// @brief In-process connection (cannot be used for inter-process communication).
    in_process = 1U << 0U,
    /// @brief Inter-process connection for local communication.
    local_interprocess = 1U << 1U,
    /// @brief Inter-process connection for remote communucation
    remote_interprocess = 1U << 2U
};
//------------------------------------------------------------------------------
/// @brief Alias for connection kind bitfield.
/// @ingroup msgbus
export using connection_kinds = bitfield<connection_kind>;

export auto operator|(const connection_kind l, const connection_kind r) noexcept
  -> connection_kinds {
    return {l, r};
}
//------------------------------------------------------------------------------
/// @brief Bus message verification bits enumeration,
/// @ingroup msgbus
/// @see verification_bits
export enum class verification_bit : std::uint8_t {
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
/// @brief Alias for a bus message verification bitfield.
/// @ingroup msgbus
export using verification_bits = bitfield<verification_bit>;

export auto operator|(
  const verification_bit l,
  const verification_bit r) noexcept -> verification_bits {
    return {l, r};
}
//------------------------------------------------------------------------------
/// @brief Message bus connection address kind enumeration.
/// @ingroup msgbus
/// @see connection_addr_kind_tag
export enum class connection_addr_kind : std::uint8_t {
    /// @brief No public address.
    none,
    /// @brief Filesystem path.
    filepath,
    /// @brief PIv4 address.
    ipv4
};

/// @brief Tag template alias for specifying connection address kind.
/// @ingroup msgbus
export template <connection_addr_kind Kind>
using connection_addr_kind_tag =
  std::integral_constant<connection_addr_kind, Kind>;
//------------------------------------------------------------------------------
/// @brief Message bus connection protocol.
/// @ingroup msgbus
/// @see connection_protocol_tag
export enum class connection_protocol : std::uint8_t {
    /// @brief Reliable stream protocol.
    stream,
    /// @brief Datagram protocol.
    datagram,
    /// @brief Message protocol.
    message
};

/// @brief Tag template alias for specifying connection protocol kind.
/// @ingroup msgbus
/// @see stream_protocol_tag
/// @see datagram_protocol_tag
export template <connection_protocol Proto>
using connection_protocol_tag =
  std::integral_constant<connection_protocol, Proto>;

/// @brief Tag type for specifying stream connection protocols.
/// @ingroup msgbus
/// @see datagram_protocol_tag
export using stream_protocol_tag =
  connection_protocol_tag<connection_protocol::stream>;

/// @brief Tag type for specifying datagram connection protocols.
/// @ingroup msgbus
/// @see stream_protocol_tag
export using datagram_protocol_tag =
  connection_protocol_tag<connection_protocol::datagram>;
//------------------------------------------------------------------------------
/// @brief The minimum guaranteed block size that can be sent through bus connections.
/// @ingroup msgbus
export constexpr const span_size_t min_connection_data_size = 4096;
//------------------------------------------------------------------------------
/// @brief Alias for message sequence number type.
/// @ingroup msgbus
export using message_sequence_t = std::uint32_t;
//------------------------------------------------------------------------------
/// @brief Structure holding part of router connection topology information.
/// @ingroup msgbus
export struct router_topology_info {
    /// @brief The router message bus id.
    identifier_t router_id{0};

    /// @brief The remote node message bus id.
    identifier_t remote_id{0};

    /// @brief The router process instance id.
    process_instance_id_t instance_id{0U};

    /// @brief The connection kind.
    connection_kind connect_kind{0U};
};

export template <typename Selector>
constexpr auto data_member_mapping(
  const std::type_identity<router_topology_info>,
  const Selector) noexcept {
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
export struct router_statistics {
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

export template <typename Selector>
constexpr auto data_member_mapping(
  const std::type_identity<router_statistics>,
  const Selector) noexcept {
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
export struct bridge_topology_info {
    /// @brief The bridge message bus id.
    identifier_t bridge_id{0};

    /// @brief The remote node message bus id.
    identifier_t opposite_id{0};

    /// @brief The bridge process instance id.
    process_instance_id_t instance_id{0U};
};

export template <typename Selector>
constexpr auto data_member_mapping(
  const std::type_identity<bridge_topology_info>,
  const Selector) noexcept {
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
export struct bridge_statistics {
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

export template <typename Selector>
constexpr auto data_member_mapping(
  const std::type_identity<bridge_statistics>,
  const Selector) noexcept {
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
export struct endpoint_topology_info {
    /// @brief The endpoint message bus id.
    identifier_t endpoint_id{0U};

    /// @brief The endpoint process instance id.
    process_instance_id_t instance_id{0U};
};

export template <typename Selector>
constexpr auto data_member_mapping(
  const std::type_identity<endpoint_topology_info>,
  const Selector) noexcept {
    using S = endpoint_topology_info;
    return make_data_member_mapping<S, identifier_t, process_instance_id_t>(
      {"endpoint_id", &S::endpoint_id}, {"instance_id", &S::instance_id});
}
//------------------------------------------------------------------------------
/// @brief Structure holding endpoint statistics information.
/// @ingroup msgbus
export struct endpoint_statistics {
    /// @brief Number of sent messages.
    std::int64_t sent_messages{0};

    /// @brief Number of received messages.
    std::int64_t received_messages{0};

    /// @brief Number of dropped messages.
    std::int64_t dropped_messages{0};

    /// @brief Uptime in seconds.
    std::int64_t uptime_seconds{0};
};

export template <typename Selector>
constexpr auto data_member_mapping(
  const std::type_identity<endpoint_statistics>,
  const Selector) noexcept {
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
export struct endpoint_info {
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

    auto operator!=(const endpoint_info& r) const noexcept -> bool {
        return tie() != r.tie();
    }
};

export template <typename Selector>
constexpr auto data_member_mapping(
  const std::type_identity<endpoint_info>,
  const Selector) noexcept {
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
export struct connection_statistics {
    /// @brief The local node message bus id.
    identifier_t local_id{0};

    /// @brief The remote node message bus id.
    identifier_t remote_id{0};

    /// @brief Ratio (0.0 - 1.0) of how much of each message data block is used.
    float block_usage_ratio{-1.F};

    /// @brief Number of bytes per second transfered.
    float bytes_per_second{-1.F};
};

export template <typename Selector>
constexpr auto data_member_mapping(
  const std::type_identity<connection_statistics>,
  const Selector) noexcept {
    using S = connection_statistics;
    return make_data_member_mapping<S, identifier_t, identifier_t, float, float>(
      {"local_id", &S::local_id},
      {"remote_id", &S::remote_id},
      {"block_usage_ratio", &S::block_usage_ratio},
      {"bytes_per_second", &S::bytes_per_second});
}
//------------------------------------------------------------------------------
/// @brief Structure holding message bus data flow information.
/// @ingroup msgbus
export struct message_flow_info {
    /// @brief The average age of message in milliseconds.
    /// @see average_message_age
    std::int32_t avg_msg_age_ms{0};

    template <typename R, typename P>
    auto set_average_message_age(std::chrono::duration<R, P> age) noexcept {
        avg_msg_age_ms = limit_cast<std::int32_t>(
          std::chrono::duration_cast<std::chrono::milliseconds>(age).count());
    }

    /// @brief Returns the average message age as chrono duration
    auto average_message_age() const noexcept {
        return std::chrono::microseconds{avg_msg_age_ms * 1000};
    }

    auto operator==(const message_flow_info&) const noexcept -> bool = default;
    auto operator!=(const message_flow_info&) const noexcept -> bool = default;
};

export template <typename Selector>
constexpr auto data_member_mapping(
  const std::type_identity<message_flow_info>,
  const Selector) noexcept {
    using S = message_flow_info;
    return make_data_member_mapping<S, std::int32_t>(
      {"avg_msg_age_ms", &S::avg_msg_age_ms});
}
//------------------------------------------------------------------------------
/// @brief Alias for IPv4 port number value type.
/// @ingroup msgbus
export using ipv4_port = unsigned short int;

/// @brief Parses a IPv4 hostname:port pair,
/// @ingroup msgbus
export auto parse_ipv4_addr(const string_view addr_str) noexcept
  -> std::tuple<std::string, ipv4_port> {
    auto [hostname, port_str] = split_by_last(
      addr_str ? addr_str : string_view{"localhost"}, string_view(":"));
    return {
      to_string(hostname),
      from_string<ipv4_port>(port_str).value_or(ipv4_port{34912U})};
}
//------------------------------------------------------------------------------
/// @brief Additional flags / options for a transfered blob.
/// @ingroup msgbus
/// @see blob_option
export enum class blob_option : std::uint8_t {
    compressed = 1U << 0U,
    with_metadata = 1U << 1U
};

//------------------------------------------------------------------------------
/// @brief Alias for blob options bitfield.
/// @ingroup msgbus
/// @see blob_options
export using blob_options = bitfield<blob_option>;

export auto operator|(const blob_option l, const blob_option r) noexcept
  -> blob_options {
    return {l, r};
}
//------------------------------------------------------------------------------
/// @brief Message priority enumeration.
/// @ingroup msgbus
export enum class message_priority : std::uint8_t {
    /// @brief Idle, sent only when no messages with higher priority are enqueued.
    idle,
    /// @brief Low message priority.
    low,
    /// @brief Normal, default message priority.
    normal,
    /// @brief High, sent before messages with lower priority.
    high,
    /// @brief Critical, sent as soon as possible.
    critical
};
//------------------------------------------------------------------------------
/// @brief Message priority ordering.
/// @ingroup msgbus
/// @relates message_priority
export [[nodiscard]] auto operator<(
  const message_priority l,
  const message_priority r) noexcept -> bool {
    using U = std::underlying_type_t<message_priority>;
    return U(l) < U(r);
}
//------------------------------------------------------------------------------
/// @brief Message cryptography-related flag bits enumeration.
/// @ingroup msgbus
/// @see message_crypto_flags
export enum class message_crypto_flag : std::uint8_t {
    /// @brief Assymetric cipher is used (symmetric otherwise).
    asymmetric = 1U << 0U,
    /// @brief The message header is signed.
    signed_header = 1U << 1U,
    /// @brief The message content is signed.
    signed_content = 1U << 2U
};
/// @brief  Alias for message crypto flags bitfield.
/// @ingroup msgbus
export using message_crypto_flags = bitfield<message_crypto_flag>;
//------------------------------------------------------------------------------
} // namespace msgbus
//------------------------------------------------------------------------------
export template <typename Selector>
constexpr auto enumerator_mapping(
  const std::type_identity<msgbus::node_kind>,
  const Selector) noexcept {
    using msgbus::node_kind;
    return enumerator_map_type<node_kind, 4>{
      {{"unknown", node_kind::unknown},
       {"endpoint", node_kind::endpoint},
       {"bridge", node_kind::bridge},
       {"router", node_kind::router}}};
}
//------------------------------------------------------------------------------
export template <typename Selector>
constexpr auto enumerator_mapping(
  const std::type_identity<msgbus::connection_kind>,
  const Selector) noexcept {
    using msgbus::connection_kind;
    return enumerator_map_type<connection_kind, 4>{
      {{"unknown", connection_kind::unknown},
       {"in_process", connection_kind::in_process},
       {"local_interprocess", connection_kind::local_interprocess},
       {"remote_interprocess", connection_kind::remote_interprocess}}};
}
//------------------------------------------------------------------------------
export template <typename Selector>
constexpr auto enumerator_mapping(
  const std::type_identity<msgbus::verification_bit>,
  const Selector) noexcept {
    using msgbus::verification_bit;
    return enumerator_map_type<verification_bit, 5>{
      {{"source_id", verification_bit::source_id},
       {"source_certificate", verification_bit::source_certificate},
       {"source_private_key", verification_bit::source_private_key},
       {"message_id", verification_bit::message_id},
       {"message_content", verification_bit::message_content}}};
}
//------------------------------------------------------------------------------
export template <typename Selector>
constexpr auto enumerator_mapping(
  const std::type_identity<msgbus::connection_addr_kind>,
  const Selector) noexcept {
    using msgbus::connection_addr_kind;
    return enumerator_map_type<connection_addr_kind, 3>{
      {{"none", connection_addr_kind::none},
       {"filepath", connection_addr_kind::filepath},
       {"ipv4", connection_addr_kind::ipv4}}};
}
//------------------------------------------------------------------------------
export template <typename Selector>
constexpr auto enumerator_mapping(
  const std::type_identity<msgbus::connection_protocol>,
  const Selector) noexcept {
    using msgbus::connection_protocol;
    return enumerator_map_type<connection_protocol, 3>{
      {{"stream", connection_protocol::stream},
       {"datagram", connection_protocol::datagram},
       {"message", connection_protocol::message}}};
}
//------------------------------------------------------------------------------
export template <typename Selector>
constexpr auto enumerator_mapping(
  const std::type_identity<msgbus::blob_option>,
  const Selector) noexcept {
    using msgbus::blob_option;
    return enumerator_map_type<blob_option, 2>{
      {{"compressed", blob_option::compressed},
       {"with_metadata", blob_option::with_metadata}}};
}
//------------------------------------------------------------------------------
export template <typename Selector>
constexpr auto enumerator_mapping(
  const std::type_identity<msgbus::message_priority>,
  const Selector) noexcept {
    using msgbus::message_priority;
    return enumerator_map_type<message_priority, 5>{
      {{"critical", message_priority::critical},
       {"high", message_priority::high},
       {"normal", message_priority::normal},
       {"low", message_priority::low},
       {"idle", message_priority::idle}}};
}
//------------------------------------------------------------------------------
export template <typename Selector>
constexpr auto enumerator_mapping(
  const std::type_identity<msgbus::message_crypto_flag>,
  const Selector) noexcept {
    using msgbus::message_crypto_flag;
    return enumerator_map_type<message_crypto_flag, 3>{
      {{"asymmetric", message_crypto_flag::asymmetric},
       {"signed_header", message_crypto_flag::signed_header},
       {"signed_content", message_crypto_flag::signed_content}}};
}
//------------------------------------------------------------------------------
} // namespace eagine

/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

export module eagine.msgbus.core:remote_node;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.build_info;
import eagine.core.identifier;
import eagine.core.reflection;
import eagine.core.container;
import eagine.core.units;
import eagine.core.utility;
import eagine.core.valid_if;
import eagine.core.logging;
import eagine.core.main_ctx;
import :types;

namespace eagine {
namespace msgbus {
//------------------------------------------------------------------------------
/// @brief Enumeration of changes tracked about remote message bus nodes.
/// @ingroup msgbus
/// @see remote_node_changes
/// @see remote_host_change
export enum class remote_node_change : std::uint16_t {
    /// @brief The node kind has appeared or changed.
    /// @see node_kind
    kind = 1U << 0U,
    /// @brief The endpoint instance id has changed.
    instance_id = 1U << 1U,
    /// @brief The host identifier has appeared or changed.
    host_id = 1U << 2U,
    /// @brief The host information has appeared or changed.
    host_info = 1U << 3U,
    /// @brief The build information has appeared or changed.
    build_info = 1U << 4U,
    /// @brief The application information has appeared or changed.
    application_info = 1U << 5U,
    /// @brief The endpoint information has appeared or changed.
    endpoint_info = 1U << 6U,
    /// @brief New remotly callable methods have been added.
    methods_added = 1U << 7U,
    /// @brief New remotly callable methods have been removed.
    methods_removed = 1U << 8U,
    /// @brief Node started responding to pings.
    started_responding = 1U << 9U,
    /// @brief Node stopped responding to pings.
    stopped_responding = 1U << 10U,
    /// @brief Node ping response rate.
    response_rate = 1U << 11U,
    /// @brief The hardware configuration information has appeared or changed.
    hardware_config = 1U << 12U,
    /// @brief New sensor values have appeared or changed.
    sensor_values = 1U << 13U,
    /// @brief New statistic values have appeared or changed.
    statistics = 1U << 14U,
    /// @brief The bus connection information has appeared or changed.
    connection_info = 1U << 15U
};
//------------------------------------------------------------------------------
/// @brief Enumeration of changes tracked about remote message bus instances.
/// @ingroup msgbus
/// @see remote_instance_changes
/// @see remote_node_change
export enum class remote_instance_change : std::uint16_t {
    /// @brief The host identifier has appeared or changed.
    host_id = 1U << 0U,
    /// @brief Instance started responding.
    started_responding = 1U << 1U,
    /// @brief Instance stopped responding.
    stopped_responding = 1U << 2U,
    /// @brief The build information has appeared or changed.
    build_info = 1U << 3U,
    /// @brief The application information has appeared or changed.
    application_info = 1U << 4U,
    /// @brief New statistics have appeared or changed.
    statistics = 1U << 5U
};
//------------------------------------------------------------------------------
/// @brief Enumeration of changes tracked about remote message bus hosts.
/// @ingroup msgbus
/// @see remote_host_changes
/// @see remote_node_change
export enum class remote_host_change : std::uint16_t {
    /// @brief The host name has appeared or changed.
    hostname = 1U << 0U,
    /// @brief Host started responding.
    started_responding = 1U << 1U,
    /// @brief Host stopped responding.
    stopped_responding = 1U << 2U,
    /// @brief The hardware configuration information has appeared or changed.
    hardware_config = 1U << 3U,
    /// @brief New sensor values have appeared or changed.
    sensor_values = 1U << 4U
};
//------------------------------------------------------------------------------
/// @brief Class providing and manipulating information about remote node changes.
/// @ingroup msgbus
/// @see remote_host_changes
/// @see remote_instance_changes
export struct remote_node_changes : bitfield<remote_node_change> {
    using base = bitfield<remote_node_change>;
    using base::base;

    /// @brief Remote node responsivity has changed.
    auto responsivity() const noexcept -> bool {
        return has_any(
          remote_node_change::started_responding,
          remote_node_change::stopped_responding);
    }

    /// @brief Remote node instance id has changed.
    auto new_instance() const noexcept -> bool {
        return has(remote_node_change::instance_id);
    }
};

export constexpr auto operator|(
  remote_node_change l,
  remote_node_change r) noexcept -> remote_node_changes {
    return {l, r};
}
//------------------------------------------------------------------------------
/// @brief Class providing and manipulating information about remote instance changes.
/// @ingroup msgbus
/// @see remote_node_changes
/// @see remote_host_changes
export struct remote_instance_changes : bitfield<remote_instance_change> {
    using base = bitfield<remote_instance_change>;
    using base::base;

    /// @brief Remote instance responsivity has changed.
    auto responsivity() const noexcept -> bool {
        return has_any(
          remote_instance_change::started_responding,
          remote_instance_change::stopped_responding);
    }
};

export constexpr auto operator|(
  const remote_instance_change l,
  const remote_instance_change r) noexcept -> remote_instance_changes {
    return {l, r};
}
//------------------------------------------------------------------------------
/// @brief Class providing and manipulating information about remote host changes.
/// @ingroup msgbus
/// @see remote_node_changes
/// @see remote_instance_changes
export struct remote_host_changes : bitfield<remote_host_change> {
    using base = bitfield<remote_host_change>;
    using base::base;

    /// @brief Remote host responsivity has changed.
    auto responsivity() const noexcept -> bool {
        return has_any(
          remote_host_change::started_responding,
          remote_host_change::stopped_responding);
    }
};

export constexpr auto operator|(
  const remote_host_change l,
  const remote_host_change r) noexcept -> remote_host_changes {
    return {l, r};
}
//------------------------------------------------------------------------------
class remote_node_tracker_impl;
class remote_host_impl;
class remote_instance_impl;
class remote_node_impl;
class node_connection_impl;
export class remote_host;
export class remote_instance;
export class remote_instance_state;
export class remote_host_state;
export class remote_node;
export class remote_node_state;
export class node_connection_state;
export class node_connection;
export class node_connections;
//------------------------------------------------------------------------------
/// @brief Class tracking the state of remote message bus nodes.
/// @ingroup msgbus
/// @see remote_node_changes
/// @see remote_host_changes
export class remote_node_tracker {
public:
    /// @brief Default constructor.
    remote_node_tracker() noexcept;

    remote_node_tracker(nothing_t) noexcept
      : _pimpl{} {}

    remote_node_tracker(shared_holder<remote_node_tracker_impl> pimpl) noexcept
      : _pimpl{std::move(pimpl)} {}

    auto cached(const std::string&) noexcept -> string_view;

    /// @brief Finds and returns the state information about a remote bus node.
    /// @see get_host
    /// @see get_instance
    /// @see get_connection
    /// @see for_each_node
    /// @see remove_node
    auto get_node(const endpoint_id_t node_id) noexcept -> remote_node_state&;

    /// @brief Removes tracked node with the specified id.
    /// @see get_node
    auto remove_node(const endpoint_id_t node_id) noexcept -> bool;

    /// @brief Finds and returns the state information about a remote host.
    /// @see get_node
    /// @see get_instance
    /// @see get_connection
    /// @see for_each_host
    auto get_host(const host_id_t) noexcept -> remote_host_state&;

    /// @brief Finds and returns the state information about a remote host.
    /// @see get_node
    /// @see get_instance
    /// @see get_connection
    /// @see for_each_host
    auto get_host(const host_id_t) const noexcept -> remote_host_state;

    /// @brief Finds and returns the information about a remote instance (process).
    /// @see get_node
    /// @see get_host
    /// @see get_connection
    auto get_instance(const process_instance_id_t) noexcept
      -> remote_instance_state&;

    /// @brief Finds and returns the information about a remote instance (process).
    /// @see get_node
    /// @see get_host
    /// @see get_connection
    auto get_instance(const process_instance_id_t) const noexcept
      -> remote_instance_state;

    /// @brief Finds and returns the information about remote node connections.
    /// @see get_node
    /// @see get_host
    /// @see get_instance
    /// @see for_each_connection
    auto get_connection(
      const endpoint_id_t node_id1,
      const endpoint_id_t node_id2) noexcept -> node_connection_state&;

    /// @brief Finds and returns the information about remote node connections.
    /// @see get_node
    /// @see get_host
    /// @see get_instance
    /// @see for_each_connection
    auto get_connection(
      const endpoint_id_t node_id1,
      const endpoint_id_t node_id2) const noexcept -> node_connection_state;

    auto notice_instance(
      const endpoint_id_t node_id,
      const process_instance_id_t) noexcept -> remote_node_state&;

    /// @brief Calls a function on each tracked remote host.
    /// @see remote_host
    /// @see for_each_host_state
    /// @see for_each_node
    ///
    /// The function is called with (host_id_t, const remote_host&) as arguments.
    template <typename Function>
    void for_each_host(Function func);

    /// @brief Calls a function on each tracked remote host.
    /// @see remote_host_state
    /// @see for_each_host
    /// @see for_each_node
    ///
    /// The function is called with (host_id_t, const remote_host_state&) as arguments.
    /// This function is subject to change without notice. Prefer using for_each_host.
    template <typename Function>
    void for_each_host_state(Function func);

    /// @brief Calls a function on each tracked remote bus node.
    /// @see remote_node
    /// @see for_each_node_state
    /// @see for_each_host
    ///
    /// The function is called with (host_id_t, const remote_node&) as arguments.
    template <typename Function>
    void for_each_node(Function func);

    /// @brief Calls a function on each tracked remote bus node.
    /// @see remote_node_state
    /// @see for_each_node
    /// @see for_each_host
    ///
    /// The function is called with (host_id_t, const remote_node_state&) as arguments.
    /// This function is subject to change without notice. Prefer using for_each_node.
    template <typename Function>
    void for_each_node_state(Function func);

    /// @brief Calls a function on each tracked remote bus instance.
    /// @see remote_instance_state
    /// @see for_each_node_state
    /// @see for_each_host_state
    ///
    /// The function is called with (process_instance_id_t, const remote_instance_state&)
    /// as arguments. This function is subject to change without notice.
    template <typename Function>
    void for_each_instance_state(Function func);

    /// @brief Calls a function on tracked remote bus nodes of an instance (process).
    /// @see remote_node_state
    /// @see for_each_node_state
    /// @see for_each_host_node_state
    ///
    /// The function is called with (host_id_t, const remote_node_state&) as arguments.
    /// This function is subject to change without notice. Prefer using for_each_node.
    template <typename Function>
    void for_each_instance_node_state(
      const process_instance_id_t inst_id,
      Function func);

    /// @brief Calls a function on tracked remote bus nodes of a remote host.
    /// @see remote_node_state
    /// @see for_each_node_state
    /// @see for_each_instance_node_state
    ///
    /// The function is called with (host_id_t, const remote_node_state&) as arguments.
    /// This function is subject to change without notice. Prefer using for_each_node.
    template <typename Function>
    void for_each_host_node_state(const host_id_t host_id, Function func);

    template <typename Function>
    void for_each_connection(Function func);

    /// @brief Calls a function on tracked connections between bus nodes.
    /// @see node_connection_state
    /// @see for_each_node_state
    ///
    /// The function is called with (const node_connection&) as argument.
    template <typename Function>
    void for_each_connection(Function func) const;

private:
    friend class node_connections;

    auto _get_nodes() noexcept -> flat_map<endpoint_id_t, remote_node_state>&;
    auto _get_instances() noexcept
      -> flat_map<process_instance_id_t, remote_instance_state>&;
    auto _get_hosts() noexcept -> flat_map<host_id_t, remote_host_state>&;
    auto _get_connections() noexcept -> std::vector<node_connection_state>&;
    auto _get_connections() const noexcept
      -> const std::vector<node_connection_state>&;

    shared_holder<remote_node_tracker_impl> _pimpl{};
};
//------------------------------------------------------------------------------
/// @brief Class providing information about a remote host of bus nodes.
/// @ingroup msgbus
/// @see remote_node_tracker
/// @see remote_node
/// @see remote_instance
/// @see node_connection
export class remote_host {
public:
    remote_host() noexcept = default;
    remote_host(const host_id_t host_id) noexcept
      : _host_id{host_id} {}

    /// @brief Indicates if this is not-empty and has actual information.
    explicit operator bool() const noexcept {
        return bool(_pimpl);
    }

    /// @brief Returns the unique host id.
    auto id() const noexcept -> valid_if_not_zero<host_id_t> {
        return {_host_id};
    }

    /// @brief Indicates if the remote host is reachable/alive.
    auto is_alive() const noexcept -> bool;

    /// @brief Returns the name of the remote host.
    auto name() const noexcept -> valid_if_not_empty<string_view>;

    /// @brief Returns the number of concurrent threads supported at the host.
    auto cpu_concurrent_threads() const noexcept
      -> valid_if_positive<span_size_t>;

    /// @brief Returns the short average load on the remote host.
    /// @see long_average_load
    /// @see short_average_load_change
    /// @see system_info::short_average_load
    auto short_average_load() const noexcept -> valid_if_nonnegative<float>;

    /// @brief Returns the change in short average load on the remote host.
    /// @see short_average_load
    auto short_average_load_change() const noexcept -> optionally_valid<float>;

    /// @brief Returns the long average load on the remote host.
    /// @see short_average_load
    /// @see system_info::long_average_load
    auto long_average_load() const noexcept -> valid_if_nonnegative<float>;

    /// @brief Returns the change in long average load on the remote host.
    /// @see long_average_load
    auto long_average_load_change() const noexcept -> optionally_valid<float>;

    /// @brief Returns the total RAM size on the remote host.
    /// @see free_ram_size
    /// @see total_swap_size
    /// @see ram_usage
    /// @see system_info::total_ram_size
    auto total_ram_size() const noexcept -> valid_if_positive<span_size_t>;

    /// @brief Returns the free RAM size on the remote host.
    /// @see total_ram_size
    /// @see free_swap_size
    /// @see free_ram_size_change
    /// @see ram_usage
    /// @see system_info::free_ram_size
    auto free_ram_size() const noexcept -> valid_if_positive<span_size_t>;

    /// @brief Returns the change in free RAM size on the remote host.
    /// @see free_ram_size
    auto free_ram_size_change() const noexcept -> optionally_valid<span_size_t>;

    /// @brief Returns the RAM usage on the remote host (0.0, 1.0).
    /// @see ram_usage_change
    /// @see total_ram_size
    /// @see free_ram_size
    auto ram_usage() const noexcept -> valid_if_nonnegative<float> {
        return meld(free_ram_size(), total_ram_size())
          .transform([](const auto free, const auto total) {
              return 1.F - float(free) / float(total);
          })
          .value_or(-1.F);
    }

    /// @brief Returns the change in RAM usage on the remote host (-1.0, 1.0).
    /// @see ram_usage
    auto ram_usage_change() const noexcept -> optionally_valid<float> {
        return meld(free_ram_size_change(), total_ram_size())
          .and_then(
            [](const auto change, const auto total) -> optionally_valid<float> {
                return {-float(total) / float(total), true};
            });
    }

    /// @brief Returns the total swap size on the remote host.
    /// @see free_swap_size
    /// @see total_ram_size
    /// @see free_swap_size_change
    /// @see ram_usage
    /// @see swap_usage
    /// @see system_info::total_swap_size
    auto total_swap_size() const noexcept -> valid_if_positive<span_size_t>;

    /// @brief Returns the free swap size on the remote host.
    /// @see total_swap_size
    /// @see free_ram_size
    /// @see free_swap_size_change
    /// @see swap_usage
    /// @see system_info::total_ram_size
    auto free_swap_size() const noexcept -> valid_if_nonnegative<span_size_t>;

    /// @brief Returns the change in free swap size on the remote host.
    /// @see free_swap_size
    auto free_swap_size_change() const noexcept
      -> optionally_valid<span_size_t>;

    /// @brief Returns the swap usage on the remote host (0.0, 1.0).
    /// @see total_swap_size
    /// @see free_swap_size
    auto swap_usage() const noexcept -> valid_if_nonnegative<float> {
        return meld(free_swap_size(), total_swap_size())
          .transform([](const auto free, const auto total) {
              return 1.F - float(free) / float(total);
          })
          .value_or(-1.F);
    }

    /// @brief Returns the change in swap usage on the remote host (-1.0, 1.0).
    /// @see swap_usage
    auto swap_usage_change() const noexcept -> optionally_valid<float> {
        return meld(free_swap_size_change(), total_swap_size())
          .and_then(
            [](const auto change, const auto total) -> optionally_valid<float> {
                return {-float(total) / float(total), true};
            });
    }

    /// @brief Returns the minimum temperature recorded on the remote host.
    /// @see max_temperature
    /// @see min_temperature_change
    auto min_temperature() const noexcept
      -> valid_if_positive<kelvins_t<float>>;

    /// @brief Returns the maximum temperature recorded on the remote host.
    /// @see min_temperature
    /// @see max_temperature_change
    auto max_temperature() const noexcept
      -> valid_if_positive<kelvins_t<float>>;

    /// @brief Returns the change in minimum temperature on the remote host.
    /// @see min_temperature
    auto min_temperature_change() const noexcept
      -> optionally_valid<kelvins_t<float>>;

    /// @brief Returns the change in maximum temperature on the remote host.
    /// @see max_temperature
    auto max_temperature_change() const noexcept
      -> optionally_valid<kelvins_t<float>>;

    /// @brief Returns the power supply kind used on the remote host.
    auto power_supply() const noexcept -> power_supply_kind;

protected:
    auto _impl() const noexcept -> optional_reference<const remote_host_impl>;
    auto _impl() noexcept -> optional_reference<remote_host_impl>;

private:
    host_id_t _host_id{0U};
    shared_holder<remote_host_impl> _pimpl{};
};
//------------------------------------------------------------------------------
/// @brief Class manipulating information about a remote host of bus nodes.
/// @ingroup msgbus
/// @see remote_node_tracker
/// @see remote_node
/// @note Do not use directly. Use remote_host instead.
export class remote_host_state : public remote_host {
public:
    using remote_host::remote_host;

    auto update() noexcept -> remote_host_state&;
    auto changes() noexcept -> remote_host_changes;
    auto add_change(const remote_host_change) noexcept -> remote_host_state&;

    auto should_query_sensors() const noexcept -> bool;
    auto sensors_queried() noexcept -> remote_host_state&;

    auto notice_alive() noexcept -> remote_host_state&;
    auto set_hostname(std::string) noexcept -> remote_host_state&;
    auto set_cpu_concurrent_threads(const span_size_t) noexcept
      -> remote_host_state&;
    auto set_short_average_load(const float) noexcept -> remote_host_state&;
    auto set_long_average_load(const float) noexcept -> remote_host_state&;
    auto set_total_ram_size(const span_size_t) noexcept -> remote_host_state&;
    auto set_total_swap_size(const span_size_t) noexcept -> remote_host_state&;
    auto set_free_ram_size(const span_size_t) noexcept -> remote_host_state&;
    auto set_free_swap_size(const span_size_t) noexcept -> remote_host_state&;
    auto set_temperature_min_max(
      const kelvins_t<float> min,
      const kelvins_t<float> max) noexcept -> remote_host_state&;
    auto set_power_supply(const power_supply_kind) noexcept
      -> remote_host_state&;
};
//------------------------------------------------------------------------------
/// @brief Class providing information about a remote instance running bus nodes.
/// @ingroup msgbus
/// @see remote_node_tracker
/// @see remote_node
/// @see remote_host
/// @see node_connection
export class remote_instance {
public:
    remote_instance() noexcept = default;
    remote_instance(
      const process_instance_id_t inst_id,
      remote_node_tracker tracker) noexcept
      : _inst_id{inst_id}
      , _tracker{std::move(tracker)} {}

    /// @brief Indicates if this is not-empty and has actual information.
    explicit operator bool() const noexcept {
        return bool(_pimpl);
    }

    /// @brief Returns the id of the instance unique in the host scope.
    auto id() const noexcept -> valid_if_not_zero<process_instance_id_t> {
        return {_inst_id};
    }

    /// @brief Indicates if the remote instance (process) is alive and responsive.
    auto is_alive() const noexcept -> bool;

    /// @brief Returns the information about the host where the instance is running.
    auto host() const noexcept -> remote_host;

    /// @brief Returns the application name of this instance.
    auto application_name() const noexcept -> valid_if_not_empty<string_view>;

    /// @brief Returns the compiler information about the program running in the instance.
    auto compiler() const noexcept -> optional_reference<const compiler_info>;

    /// @brief Returns the build information about the program running in the instance.
    auto build_version() const noexcept
      -> optional_reference<const version_info>;

private:
    process_instance_id_t _inst_id{0U};
    shared_holder<remote_instance_impl> _pimpl{};

protected:
    remote_node_tracker _tracker{nothing};
    auto _impl() const noexcept
      -> optional_reference<const remote_instance_impl>;
    auto _impl() noexcept -> optional_reference<remote_instance_impl>;
};
//------------------------------------------------------------------------------
/// @brief Class manipulating information about a remote instance running bus nodes.
/// @ingroup msgbus
/// @see remote_node_tracker
/// @see remote_instance
/// @note Do not use directly. Use remote_host instead.
export class remote_instance_state : public remote_instance {
public:
    using remote_instance::remote_instance;

    auto update() noexcept -> remote_instance_state&;
    auto changes() noexcept -> remote_instance_changes;
    auto add_change(const remote_instance_change) noexcept
      -> remote_instance_state&;

    auto notice_alive() noexcept -> remote_instance_state&;
    auto set_host_id(const host_id_t) noexcept -> remote_instance_state&;
    auto set_app_name(const std::string&) noexcept -> remote_instance_state&;
    auto assign(compiler_info) noexcept -> remote_instance_state&;
    auto assign(version_info) noexcept -> remote_instance_state&;
};
//------------------------------------------------------------------------------
/// @brief Class providing information about a remote bus node.
/// @ingroup msgbus
/// @see remote_node_tracker
/// @see remote_host
/// @see remote_instance
/// @see node_connection
export class remote_node {
public:
    remote_node() noexcept = default;
    remote_node(
      const endpoint_id_t node_id,
      remote_node_tracker tracker) noexcept
      : _node_id{node_id}
      , _tracker{std::move(tracker)} {}

    /// @brief Indicates if this is not-empty and has actual information.
    explicit operator bool() const noexcept {
        return bool(_pimpl);
    }

    /// @brief Returns the unique id of the remote bus node.
    auto id() const noexcept -> valid_if_not_zero<endpoint_id_t> {
        return {_node_id};
    }

    /// @brief Returns the id of the instance in which the node is running.
    auto instance_id() const noexcept
      -> valid_if_not_zero<process_instance_id_t>;

    /// @brief Returns the id of the host on which the node is running.
    auto host_id() const noexcept -> valid_if_not_zero<host_id_t>;

    /// @brief Returns the kind of the remote node.
    /// @see has_known_kind
    /// @see is_router_node
    /// @see is_bridge_node
    auto kind() const noexcept -> node_kind;

    /// @brief Indicates if the kind of the remote node is known.
    /// @see kind
    /// @see is_router_node
    /// @see is_bridge_node
    auto has_known_kind() const noexcept -> bool {
        return kind() != node_kind::unknown;
    }

    /// @brief Returns if the remote node is a router control node.
    /// @see is_bridge_node
    /// @see kind
    auto is_router_node() const noexcept -> tribool;

    /// @brief Returns if the remote node is a router control node.
    /// @see is_router_node
    /// @see kind
    auto is_bridge_node() const noexcept -> tribool;

    /// @brief Indicates if endpoint information is available.
    auto has_endpoint_info() const noexcept -> bool;

    /// @brief Returns the user-readable display name of the application.
    auto display_name() const noexcept -> valid_if_not_empty<string_view>;

    /// @brief Returns the user-readable description of the application.
    auto description() const noexcept -> valid_if_not_empty<string_view>;

    /// @brief Indicates if the remote node subscribes to the specified message type.
    auto subscribes_to(message_id msg_id) const noexcept -> tribool;

    /// @brief Indicates if the remote node can query system info
    /// @see subscribes_to
    auto can_query_system_info() const noexcept -> tribool;

    /// @brief Indicates if the remote node is pingable.
    /// @see set_ping_interval
    /// @see is_responsive
    auto is_pingable() const noexcept -> tribool;

    /// @brief Sets the ping interval for the remote node.
    /// @see is_pingable
    void set_ping_interval(const std::chrono::milliseconds) noexcept;

    /// @brief Returns the last ping roundtrip time.
    /// @see is_responsive
    /// @see is_pingable
    /// @see ping_success_rate
    auto ping_roundtrip_time() const noexcept
      -> valid_if_not_zero<std::chrono::microseconds>;

    /// @brief Returns the ping success rate for the remote node (0.0, 1.0).
    /// @see is_responsive
    /// @see is_pingable
    /// @see ping_roundtrip_time
    auto ping_success_rate() const noexcept -> valid_if_between_0_1<float>;

    /// @brief Indicates if the remote node is responsive.
    /// @see ping_success_rate
    auto is_responsive() const noexcept -> tribool;

    /// @brief Returns information about the host where the node is running.
    /// @see instance
    /// @see connections
    auto host() const noexcept -> remote_host;

    /// @brief Returns information about the instance in which the node is running.
    /// @see host
    /// @see connections
    auto instance() const noexcept -> remote_instance;

    /// @brief Returns the total number of messages sent or forwarded by node.
    auto sent_messages() const noexcept -> valid_if_nonnegative<std::int64_t>;

    /// @brief Returns the total number of messages received by node.
    auto received_messages() const noexcept
      -> valid_if_nonnegative<std::int64_t>;

    /// @brief Returns the total number of messages dropped by node.
    auto dropped_messages() const noexcept
      -> valid_if_nonnegative<std::int64_t>;

    /// @brief Returns the number of messages sent or forwarded per second.
    auto messages_per_second() const noexcept -> valid_if_nonnegative<int>;

    /// @brief Returns the average message age.
    auto average_message_age() const noexcept
      -> valid_if_not_zero<std::chrono::microseconds>;

    /// @brief Returns node uptime in seconds.
    auto uptime() const noexcept -> valid_if_not_zero<std::chrono::seconds>;

    /// @brief Return information about the connections of this node.
    /// @see host
    /// @see instance
    auto connections() const noexcept -> node_connections;

private:
    endpoint_id_t _node_id{};
    shared_holder<remote_node_impl> _pimpl{};

protected:
    remote_node_tracker _tracker{nothing};
    auto _impl() const noexcept -> optional_reference<const remote_node_impl>;
    auto _impl() noexcept -> optional_reference<remote_node_impl>;
};
//------------------------------------------------------------------------------
/// @brief Class manipulating information about a remote bus node.
/// @ingroup msgbus
/// @see remote_node_tracker
/// @see remote_node
/// @note Do not use directly. Use remote_node instead
export class remote_node_state : public remote_node {
public:
    using remote_node::remote_node;

    auto clear() noexcept -> remote_node_state&;

    auto host_state() const noexcept -> remote_host_state;
    auto instance_state() const noexcept -> remote_instance_state;

    auto update() noexcept -> remote_node_state&;
    auto changes() noexcept -> remote_node_changes;
    auto add_change(const remote_node_change) noexcept -> remote_node_state&;

    auto set_instance_id(const process_instance_id_t) noexcept
      -> remote_node_state&;
    auto set_host_id(const host_id_t) noexcept -> remote_node_state&;

    auto assign(const node_kind) noexcept -> remote_node_state&;
    auto assign(const endpoint_info&) noexcept -> remote_node_state&;

    auto assign(const router_statistics&) noexcept -> remote_node_state&;
    auto assign(const bridge_statistics&) noexcept -> remote_node_state&;
    auto assign(const endpoint_statistics&) noexcept -> remote_node_state&;

    auto add_subscription(const message_id) noexcept -> remote_node_state&;
    auto remove_subscription(const message_id) noexcept -> remote_node_state&;

    auto should_ping() noexcept -> std::tuple<bool, std::chrono::milliseconds>;
    auto notice_alive() noexcept -> remote_node_state&;
    auto pinged() noexcept -> remote_node_state&;
    auto ping_response(
      const message_sequence_t,
      const std::chrono::microseconds age) noexcept -> remote_node_state&;
    auto ping_timeout(
      const message_sequence_t,
      const std::chrono::microseconds age) noexcept -> remote_node_state&;
};
//------------------------------------------------------------------------------
/// @brief Class providing information about connection between bus nodes.
/// @ingroup msgbus
/// @see remote_node_tracker
/// @see remote_node
/// @see remote_host
/// @see remote_instance
/// @see node_connections
export class node_connection {
public:
    node_connection() noexcept = default;
    node_connection(
      const endpoint_id_t id1,
      const endpoint_id_t id2,
      remote_node_tracker tracker) noexcept
      : _id1{id1}
      , _id2{id2}
      , _tracker{std::move(tracker)} {}

    /// @brief Indicates if this is not-empty and has actual information.
    explicit operator bool() const noexcept {
        return bool(_pimpl);
    }

    /// @brief Indicates if the connection connects node with the specified id.
    /// @see opposite_id
    auto connects(const endpoint_id_t id) const noexcept {
        return (_id1 == id) or (_id2 == id);
    }

    /// @brief Indicates if the connection connects nodes with the specified id.
    /// @see opposite_id
    auto connects(const endpoint_id_t id1, const endpoint_id_t id2)
      const noexcept {
        return ((_id1 == id1) and (_id2 == id2)) or
               ((_id1 == id2) and (_id2 == id1));
    }

    /// @brief Returns the id of the node opposite to the node with id in argument.
    /// @see connects
    auto opposite_id(const endpoint_id_t id) const noexcept
      -> valid_if_not_zero<endpoint_id_t> {
        if(_id1 == id) {
            return {_id2};
        }
        if(_id2 == id) {
            return {_id1};
        }
        return {0U};
    }

    /// @brief Returns the connection kind.
    auto kind() const noexcept -> connection_kind;

    /// @brief Returns the message block usage ratio for the connection.
    auto block_usage_ratio() const noexcept -> valid_if_nonnegative<float>;

    /// @brief Returns count of bytes per second sent through the connection.
    auto bytes_per_second() const noexcept -> valid_if_nonnegative<float>;

private:
    shared_holder<node_connection_impl> _pimpl{};

protected:
    endpoint_id_t _id1{0U};
    endpoint_id_t _id2{0U};
    remote_node_tracker _tracker{nothing};
    auto _impl() const noexcept
      -> optional_reference<const node_connection_impl>;
    auto _impl() noexcept -> optional_reference<node_connection_impl>;
};
//------------------------------------------------------------------------------
/// @brief Class manipulating information about connection between bus nodes.
/// @ingroup msgbus
/// @see remote_node_tracker
/// @see remote_node
/// @see remote_host
/// @see remote_instance
/// @note Do not use directly. Use node_connection instead.
export class node_connection_state : public node_connection {
public:
    using node_connection::node_connection;

    auto set_kind(const connection_kind) noexcept -> node_connection_state&;
    auto assign(const connection_statistics&) noexcept
      -> node_connection_state&;
};
//------------------------------------------------------------------------------
/// @brief Class providing information about connections from the perspective of a node.
/// @ingroup msgbus
/// @see remote_node_tracker
/// @see remote_node
/// @see remote_host
/// @see remote_instance
/// @see node_connection
export class node_connections {
public:
    node_connections(
      endpoint_id_t origin_id,
      std::vector<endpoint_id_t> remote_ids,
      remote_node_tracker tracker) noexcept
      : _origin_id{origin_id}
      , _remote_ids{std::move(remote_ids)}
      , _tracker{std::move(tracker)} {}

    /// @brief Returns the origin node connected by the listed connections.
    auto origin() -> remote_node {
        return _tracker.get_node(_origin_id);
    }

    /// @brief Returns the number of adjacent connections of the origin node.
    /// @see get
    /// @see remote
    auto count() const noexcept -> span_size_t {
        return span_size(_remote_ids.size());
    }

    /// @brief Returns the i-th connection of the origin node.
    /// @see count
    /// @see remote
    /// @pre index >= 0 and  index < count()
    auto get(const span_size_t index) noexcept -> node_connection {
        assert((index >= 0) and (index < count()));
        return _tracker.get_connection(_origin_id, _remote_ids[integer(index)]);
    }

    /// @brief Returns the node connected through the i-th connection.
    /// @see count
    /// @see get
    /// @pre index >= 0 and  index < count()
    auto remote(const span_size_t index) noexcept -> remote_node {
        assert((index >= 0) and (index < count()));
        return _tracker.get_node(_remote_ids[integer(index)]);
    }

private:
    endpoint_id_t _origin_id{};
    std::vector<endpoint_id_t> _remote_ids{};
    remote_node_tracker _tracker{nothing};
};
//------------------------------------------------------------------------------
template <typename Function>
void remote_node_tracker::for_each_host(Function func) {
    if(_pimpl) [[likely]] {
        for(auto& [host_id, host] : _get_hosts()) {
            func(host_id, static_cast<remote_host&>(host));
        }
    }
}
//------------------------------------------------------------------------------
template <typename Function>
void remote_node_tracker::for_each_host_state(Function func) {
    if(_pimpl) [[likely]] {
        for(auto& [host_id, host] : _get_hosts()) {
            func(host_id, host);
        }
    }
}
//------------------------------------------------------------------------------
template <typename Function>
void remote_node_tracker::for_each_node(Function func) {
    if(_pimpl) [[likely]] {
        for(auto& [node_id, node] : _get_nodes()) {
            func(node_id, static_cast<remote_node&>(node));
        }
    }
}
//------------------------------------------------------------------------------
template <typename Function>
void remote_node_tracker::for_each_node_state(Function func) {
    if(_pimpl) [[likely]] {
        for(auto& [node_id, node] : _get_nodes()) {
            func(node_id, node);
        }
    }
}
//------------------------------------------------------------------------------
template <typename Function>
void remote_node_tracker::for_each_instance_state(Function func) {
    if(_pimpl) [[likely]] {
        for(auto& [inst_id, inst] : _get_instances()) {
            func(inst_id, inst);
        }
    }
}
//------------------------------------------------------------------------------
template <typename Function>
void remote_node_tracker::for_each_instance_node_state(
  const process_instance_id_t inst_id,
  Function func) {
    if(_pimpl) [[likely]] {
        for(auto& [node_id, node] : _get_nodes()) {
            if(node.instance_id() == inst_id) {
                func(node_id, node);
            }
        }
    }
}
//------------------------------------------------------------------------------
template <typename Function>
void remote_node_tracker::for_each_host_node_state(
  const host_id_t host_id,
  Function func) {
    if(_pimpl) [[likely]] {
        for(auto& [node_id, node] : _get_nodes()) {
            if(node.host_id() == host_id) {
                func(node_id, node);
            }
        }
    }
}
//------------------------------------------------------------------------------
template <typename Function>
void remote_node_tracker::for_each_connection(Function func) {
    if(_pimpl) [[likely]] {
        for(auto& conn : _get_connections()) {
            func(conn);
        }
    }
}
//------------------------------------------------------------------------------
template <typename Function>
void remote_node_tracker::for_each_connection(Function func) const {
    if(_pimpl) [[likely]] {
        for(const auto& conn : _get_connections()) {
            func(conn);
        }
    }
}
//------------------------------------------------------------------------------
} // namespace msgbus
export template <>
struct enumerator_traits<msgbus::remote_node_change> {
    static constexpr auto mapping() noexcept {
        using msgbus::remote_node_change;
        return enumerator_map_type<remote_node_change, 16>{
          {{"kind", remote_node_change::kind},
           {"instance_id", remote_node_change::instance_id},
           {"host_id", remote_node_change::host_id},
           {"host_info", remote_node_change::host_info},
           {"build_info", remote_node_change::build_info},
           {"application_info", remote_node_change::application_info},
           {"endpoint_info", remote_node_change::endpoint_info},
           {"methods_added", remote_node_change::methods_added},
           {"methods_removed", remote_node_change::methods_removed},
           {"started_responding", remote_node_change::started_responding},
           {"stopped_responding", remote_node_change::stopped_responding},
           {"response_rate", remote_node_change::response_rate},
           {"hardware_config", remote_node_change::hardware_config},
           {"sensor_values", remote_node_change::sensor_values},
           {"statistics", remote_node_change::statistics},
           {"connection_info", remote_node_change::connection_info}}};
    }
};
//------------------------------------------------------------------------------
export template <>
struct enumerator_traits<msgbus::remote_instance_change> {
    static constexpr auto mapping() noexcept {
        using msgbus::remote_instance_change;
        return enumerator_map_type<remote_instance_change, 5>{
          {{"host_id", remote_instance_change::host_id},
           {"started_responding", remote_instance_change::started_responding},
           {"stopped_responding", remote_instance_change::stopped_responding},
           {"application_info", remote_instance_change::application_info},
           {"statistics", remote_instance_change::statistics}}};
    }
};
//------------------------------------------------------------------------------
export template <>
struct enumerator_traits<msgbus::remote_host_change> {
    static constexpr auto mapping() noexcept {
        using msgbus::remote_host_change;
        return enumerator_map_type<remote_host_change, 5>{
          {{"hostname", remote_host_change::hostname},
           {"started_responding", remote_host_change::started_responding},
           {"stopped_responding", remote_host_change::stopped_responding},
           {"hardware_config", remote_host_change::hardware_config},
           {"sensor_values", remote_host_change::sensor_values}}};
    }
};
//------------------------------------------------------------------------------
export auto adapt_entry_arg(
  const identifier name,
  const msgbus::remote_node& value) noexcept {
    struct _adapter {
        const identifier name;
        const msgbus::remote_node& value;

        void operator()(logger_backend& backend) const noexcept {
            backend.add_unsigned(
              name, "MsgBusEpId", value.id().or_default().value());

            value.instance_id().and_then(
              backend.add_unsigned("instanceId", "uint32", _1));

            backend.add_string(
              "nodeKind", "enum", enumerator_name(value.kind()));

            backend.add_adapted(
              "isRutrNode", yes_no_maybe(value.is_router_node()));
            backend.add_adapted(
              "isBrdgNode", yes_no_maybe(value.is_bridge_node()));
            backend.add_adapted(
              "isPingable", yes_no_maybe(value.is_pingable()));
            backend.add_adapted(
              "isRespnsve", yes_no_maybe(value.is_responsive()));

            value.ping_success_rate().and_then(
              backend.add_float("pingSucces", "Ratio", _1));

            value.instance().build_version().and_then(
              backend.add_adapted("buildInfo", _1));

            value.display_name().and_then(backend.add_adapted("dispName", _1));

            value.description().and_then(backend.add_adapted("descrption", _1));
        }
    };
    return _adapter{.name = name, .value = value};
}
//------------------------------------------------------------------------------
export auto adapt_entry_arg(
  const identifier name,
  const msgbus::remote_node_changes& value) noexcept {
    return adapt_entry_arg(
      name, static_cast<const bitfield<msgbus::remote_node_change>&>(value));
};
//------------------------------------------------------------------------------
export auto adapt_entry_arg(
  const identifier name,
  const msgbus::remote_host& value) noexcept {
    struct _adapter {
        const identifier name;
        const msgbus::remote_host& value;

        void operator()(logger_backend& backend) const noexcept {
            backend.add_unsigned(name, "uint64", value.id().value_or(0U));

            value.name().and_then(backend.add_string("hostname", "str", _1));
            value.cpu_concurrent_threads().and_then(
              backend.add_integer("cpuThreads", "int64", _1));
            value.total_ram_size().and_then(
              backend.add_integer("totalRAM", "ByteSize", _1));
            value.free_ram_size().and_then(
              backend.add_integer("freeRAM", "ByteSize", _1));
            value.free_swap_size().and_then(
              backend.add_integer("freeSwap", "ByteSize", _1));
            value.total_swap_size().and_then(
              backend.add_integer("totalSwap", "ByteSize", _1));
            value.ram_usage().and_then(
              backend.add_float("ramUsage", "Ratio", _1));
            value.swap_usage().and_then(
              backend.add_float("swapUsage", "Ratio", _1));
            value.short_average_load().and_then(
              backend.add_float("shortLoad", "Ratio", _1));
            value.long_average_load().and_then(
              backend.add_float("longLoad", "Ratio", _1));
        }
    };
    return _adapter{.name = name, .value = value};
}
//------------------------------------------------------------------------------
} // namespace eagine

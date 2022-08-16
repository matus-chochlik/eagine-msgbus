/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#ifndef EAGINE_LOGGING_TYPE_REMOTE_NODE_HPP
#define EAGINE_LOGGING_TYPE_REMOTE_NODE_HPP

#include "../../msgbus/remote_node.hpp"
#include <eagine/logging/type/build_info.hpp>
#include <eagine/logging/type/yes_no_maybe.hpp>

namespace eagine {
//------------------------------------------------------------------------------
static inline auto adapt_entry_arg(
  const identifier name,
  const msgbus::remote_node& value) {
    return [name, value](auto& backend) noexcept {
        backend.add_unsigned(name, "uint64", extract_or(value.id(), 0U));

        if(const auto opt_id{value.instance_id()}) {
            backend.add_unsigned("instanceId", "uint32", extract(opt_id));
        }

        backend.add_string("nodeKind", "enum", enumerator_name(value.kind()));

        backend.add_adapted("isRutrNode", yes_no_maybe(value.is_router_node()));
        backend.add_adapted("isBrdgNode", yes_no_maybe(value.is_bridge_node()));
        backend.add_adapted("isPingable", yes_no_maybe(value.is_pingable()));
        backend.add_adapted("isRespnsve", yes_no_maybe(value.is_responsive()));

        if(const auto opt_rate{value.ping_success_rate()}) {
            backend.add_float("pingSucces", "Ratio", extract(opt_rate));
        }
        if(const auto opt_bld{value.instance().build()}) {
            backend.add_adapted("buildInfo", extract(opt_bld));
        }

        if(const auto opt_name{value.display_name()}) {
            backend.add_string("dispName", "string", extract(opt_name));
        }

        if(const auto opt_desc{value.description()}) {
            backend.add_string("descrption", "string", extract(opt_desc));
        }
    };
}
//------------------------------------------------------------------------------
static inline auto adapt_entry_arg(
  const identifier name,
  const msgbus::remote_host& value) noexcept {
    return [name, value](auto& backend) {
        backend.add_unsigned(name, "uint64", extract_or(value.id(), 0U));

        if(const auto opt_name{value.name()}) {
            backend.add_string("hostname", "string", extract(opt_name));
        }

        if(const auto opt_val{value.cpu_concurrent_threads()}) {
            backend.add_integer("cpuThreads", "int64", extract(opt_val));
        }
        if(const auto opt_val{value.total_ram_size()}) {
            backend.add_integer("totalRAM", "ByteSize", extract(opt_val));
        }
        if(const auto opt_val{value.free_ram_size()}) {
            backend.add_integer("freeRAM", "ByteSize", extract(opt_val));
        }
        if(const auto opt_val{value.free_swap_size()}) {
            backend.add_integer("freeSwap", "ByteSize", extract(opt_val));
        }
        if(const auto opt_val{value.total_swap_size()}) {
            backend.add_integer("totalSwap", "ByteSize", extract(opt_val));
        }
        if(const auto opt_val{value.ram_usage()}) {
            backend.add_float("ramUsage", "Ratio", extract(opt_val));
        }
        if(const auto opt_val{value.swap_usage()}) {
            backend.add_float("swapUsage", "Ratio", extract(opt_val));
        }
        if(const auto opt_val{value.short_average_load()}) {
            backend.add_float("shortLoad", "Ratio", extract(opt_val));
        }
        if(const auto opt_val{value.long_average_load()}) {
            backend.add_float("longLoad", "Ratio", extract(opt_val));
        }
    };
}
//------------------------------------------------------------------------------
} // namespace eagine

#endif // EAGINE_LOGGING_TYPE_REMOTE_NODE_HPP

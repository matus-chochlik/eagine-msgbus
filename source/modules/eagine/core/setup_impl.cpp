/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

module eagine.msgbus.core;

import std;
import eagine.core.types;
import eagine.core.identifier;
import eagine.core.main_ctx;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
message_bus_setup::message_bus_setup(main_ctx_parent parent) noexcept
  : main_ctx_object{"MessageBus", parent}
  , _addr{parent, nothing}
  , _setup{parent, nothing} {}
//------------------------------------------------------------------------------
auto message_bus_setup::static_type_id() noexcept -> identifier {
    return "MsgBusSetp";
}
//------------------------------------------------------------------------------
void enable(main_ctx& ctx) {
    assert(ctx.setters());
    ctx.setters().and_then([&](auto& setters) {
        shared_holder<message_bus_setup> msg_bus{default_selector, ctx};
        msg_bus->configure(ctx.config());
        setters.inject(std::move(msg_bus));
    });
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

export module eagine.msgbus.core:setup;
import std;
import eagine.core.types;
import eagine.core.identifier;
import eagine.core.main_ctx;
import :types;
import :interface;
import :router_address;
import :connection_setup;

namespace eagine {
namespace msgbus {
//------------------------------------------------------------------------------
/// @brief Class providing access to basic message bus functionality.
/// @ingroup main_context
export class message_bus_setup
  : public main_ctx_service_impl<message_bus_setup>
  , public main_ctx_object {
public:
    message_bus_setup(main_ctx_parent parent) noexcept;

    static auto static_type_id() noexcept -> identifier;

    void configure(application_config& config) {
        _addr.configure(config);
        _setup.configure(config);
    }

    void setup_acceptors(msgbus::acceptor_user& target) {
        _setup.setup_acceptors(target, _addr);
    }

    void setup_connectors(msgbus::connection_user& target) {
        _setup.setup_connectors(target, _addr);
    }

private:
    msgbus::router_address _addr;
    msgbus::connection_setup _setup;
};
//------------------------------------------------------------------------------
void enable(main_ctx& ctx);

export void setup_connectors(main_ctx& ctx, connection_user& target) {
    const auto mbsetup{ctx.locate<message_bus_setup>()};
    assert(mbsetup);
    mbsetup->setup_connectors(target);
}

export void setup_acceptors(main_ctx& ctx, acceptor_user& target) {
    const auto mbsetup{ctx.locate<message_bus_setup>()};
    assert(mbsetup);
    mbsetup->setup_acceptors(target);
}
//------------------------------------------------------------------------------
} // namespace msgbus
//------------------------------------------------------------------------------
export void enable_message_bus(main_ctx& ctx) {
    return msgbus::enable(ctx);
}
//------------------------------------------------------------------------------
} // namespace eagine


/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
module;

module eagine.msgbus.core;

import std;
import eagine.core.memory;
import eagine.core.utility;
import eagine.core.main_ctx;
import eagine.sslplus;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
optional_router::optional_router(main_ctx_parent parent) noexcept
  : main_ctx_object{"OptnRouter", parent} {}
//------------------------------------------------------------------------------
auto optional_router::init(bool create) noexcept -> bool {
    if(create) {
        auto& ctx{main_context()};
        _router.emplace(ctx);
        _router->log_info("starting optional message bus router");
        _router->add_ca_certificate_pem(ca_certificate_pem(ctx));
        _router->add_certificate_pem(msgbus::router_certificate_pem(ctx));
        setup_acceptors(ctx, *_router);
        return true;
    }
    return false;
}
//------------------------------------------------------------------------------
auto optional_router::init(const string_view option_name) noexcept -> bool {
    return init(app_config().is_set(option_name));
}
//------------------------------------------------------------------------------
auto optional_router::update() noexcept -> work_done {
    some_true something_done;
    if(_router) {
        something_done(_router->update(8));
    }
    return something_done;
}
//------------------------------------------------------------------------------
void optional_router::finish() noexcept {
    if(_router) {
        _router->finish();
    }
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

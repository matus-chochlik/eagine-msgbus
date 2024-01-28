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
import eagine.core.types;
import eagine.core.identifier;
import eagine.core.logging;
import eagine.core.main_ctx;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
router_address::router_address(main_ctx_parent parent, const nothing_t) noexcept
  : main_ctx_object{"RouterAddr", parent} {}
//------------------------------------------------------------------------------
router_address::router_address(main_ctx_parent parent) noexcept
  : router_address{parent, nothing} {
    configure(app_config());
}
//------------------------------------------------------------------------------
void router_address::configure(application_config& config) {
    if(config.fetch("msgbus.router.address", _addrs)) {
        log_info("configured router address(es) ${address}")
          .arg_func([&](logger_backend& backend) {
              for(auto& addr : _addrs) {
                  backend.add_string("address", "str", addr);
              }
          });
    }
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


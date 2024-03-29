/// @example eagine/msgbus/log_certs.cpp
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
#include <eagine/logging/logger.hpp>
#include <eagine/main.hpp>
#include <eagine/msgbus/resources.hpp>

namespace eagine {
//------------------------------------------------------------------------------
auto main(main_ctx& ctx) -> int {
    ctx.log()
      .info("embedded router certificate")
      .arg("arg", msgbus::router_certificate_pem(ctx));
    ctx.log()
      .info("embedded bridge certificate")
      .arg("arg", msgbus::bridge_certificate_pem(ctx));
    ctx.log()
      .info("embedded endpoint certificate")
      .arg("arg", msgbus::endpoint_certificate_pem(ctx));
    return 0;
}
//------------------------------------------------------------------------------
} // namespace eagine

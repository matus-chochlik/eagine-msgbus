/// @example eagine/msgbus/001_log_certs.cpp
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
import eagine.core;
import eagine.msgbus;

namespace eagine {
//------------------------------------------------------------------------------
auto main(main_ctx& ctx) -> int {
    ctx.log()
      .info("embedded router certificate")
      .arg(identifier{"cert"}, msgbus::router_certificate_pem(ctx));
    ctx.log()
      .info("embedded bridge certificate")
      .arg(identifier{"cert"}, msgbus::bridge_certificate_pem(ctx));
    ctx.log()
      .info("embedded endpoint certificate")
      .arg(identifier{"cert"}, msgbus::endpoint_certificate_pem(ctx));
    return 0;
}
//------------------------------------------------------------------------------
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    return eagine::default_main(argc, argv, eagine::main);
}


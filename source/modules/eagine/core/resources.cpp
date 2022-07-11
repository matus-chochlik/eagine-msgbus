/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.core:resources;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.logging;
import eagine.core.main_ctx;
import eagine.core.resource;

namespace eagine {
//------------------------------------------------------------------------------
export auto router_certificate_pem(
  const memory::const_block embedded_blk,
  memory::buffer& buf,
  application_config& cfg,
  const logger& log) noexcept -> memory::const_block {
    return fetch_resource(
      string_view{"message bus router certificate"},
      string_view{"msgbus.router.cert_path"},
      embedded_blk,
      buf,
      cfg,
      log);
}
//------------------------------------------------------------------------------
export auto router_certificate_pem(
  const memory::const_block embedded_blk,
  main_ctx& ctx) noexcept -> memory::const_block {
    return router_certificate_pem(
      embedded_blk, ctx.scratch_space(), ctx.config(), ctx.log());
}
//------------------------------------------------------------------------------
export auto bridge_certificate_pem(
  const memory::const_block embedded_blk,
  memory::buffer& buf,
  application_config& cfg,
  const logger& log) noexcept -> memory::const_block {
    return fetch_resource(
      string_view{"message bus bridge certificate"},
      string_view{"msgbus.bridge.cert_path"},
      embedded_blk,
      buf,
      cfg,
      log);
}
//------------------------------------------------------------------------------
export auto bridge_certificate_pem(
  const memory::const_block embedded_blk,
  main_ctx& ctx) noexcept -> memory::const_block {
    return bridge_certificate_pem(
      embedded_blk, ctx.scratch_space(), ctx.config(), ctx.log());
}
//------------------------------------------------------------------------------
export auto endpoint_certificate_pem(
  const memory::const_block embedded_blk,
  memory::buffer& buf,
  application_config& cfg,
  const logger& log) noexcept -> memory::const_block {
    return fetch_resource(
      string_view{"message bus endpoint certificate"},
      string_view{"msgbus.endpoint.cert_path"},
      embedded_blk,
      buf,
      cfg,
      log);
}
//------------------------------------------------------------------------------
export auto endpoint_certificate_pem(
  const memory::const_block embedded_blk,
  main_ctx& ctx) noexcept -> memory::const_block {
    return endpoint_certificate_pem(
      embedded_blk, ctx.scratch_space(), ctx.config(), ctx.log());
}
//------------------------------------------------------------------------------
} // namespace eagine


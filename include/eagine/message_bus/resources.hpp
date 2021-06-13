/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#ifndef EAGINE_MESSAGE_BUS_RESOURCES_HPP
#define EAGINE_MESSAGE_BUS_RESOURCES_HPP

#include <eagine/resources.hpp>

namespace eagine::msgbus {
//------------------------------------------------------------------------------
auto router_certificate_pem(
  memory::const_block embedded_blk,
  memory::buffer&,
  application_config&,
  logger&) -> memory::const_block;
//------------------------------------------------------------------------------
inline auto
router_certificate_pem(memory::const_block embedded_blk, main_ctx& ctx)
  -> memory::const_block {
    return router_certificate_pem(
      embedded_blk, ctx.scratch_space(), ctx.config(), ctx.log());
}
//------------------------------------------------------------------------------
auto router_certificate_pem(main_ctx& ctx) -> memory::const_block;
//------------------------------------------------------------------------------
auto bridge_certificate_pem(
  memory::const_block embedded_blk,
  memory::buffer&,
  application_config&,
  logger&) -> memory::const_block;
//------------------------------------------------------------------------------
inline auto
bridge_certificate_pem(memory::const_block embedded_blk, main_ctx& ctx)
  -> memory::const_block {
    return bridge_certificate_pem(
      embedded_blk, ctx.scratch_space(), ctx.config(), ctx.log());
}
//------------------------------------------------------------------------------
auto bridge_certificate_pem(main_ctx& ctx) -> memory::const_block;
//------------------------------------------------------------------------------
auto endpoint_certificate_pem(
  memory::const_block embedded_blk,
  memory::buffer&,
  application_config&,
  logger&) -> memory::const_block;
//------------------------------------------------------------------------------
inline auto
endpoint_certificate_pem(memory::const_block embedded_blk, main_ctx& ctx)
  -> memory::const_block {
    return endpoint_certificate_pem(
      embedded_blk, ctx.scratch_space(), ctx.config(), ctx.log());
}
//------------------------------------------------------------------------------
auto endpoint_certificate_pem(main_ctx& ctx) -> memory::const_block;
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

#if !EAGINE_MSGBUS_LIBRARY || defined(EAGINE_IMPLEMENTING_LIBRARY)
#include <eagine/message_bus/resources.inl>
#endif

#endif // EAGINE_MESSAGE_BUS_RESOURCES_HPP

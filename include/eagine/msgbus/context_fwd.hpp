/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#ifndef EAGINE_MSGBUS_CONTEXT_FWD_HPP
#define EAGINE_MSGBUS_CONTEXT_FWD_HPP

#include <eagine/main_ctx_fwd.hpp>
#include <memory>

namespace eagine {
class application_config;
namespace msgbus {
//------------------------------------------------------------------------------
class context;
//------------------------------------------------------------------------------
using shared_context = std::shared_ptr<context>;
//------------------------------------------------------------------------------
[[nodiscard]] auto make_context(main_ctx_parent) -> std::shared_ptr<context>;
//------------------------------------------------------------------------------
} // namespace msgbus
} // namespace eagine

#endif // EAGINE_MSGBUS_CONTEXT_FWD_HPP

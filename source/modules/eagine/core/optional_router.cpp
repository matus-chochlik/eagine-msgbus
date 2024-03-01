/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.core:optional_router;

import std;
import eagine.core.memory;
import eagine.core.utility;
import eagine.core.main_ctx;
import :router;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
export class optional_router : public main_ctx_object {
public:
    optional_router(main_ctx_parent ctx) noexcept;

    auto do_init(bool create) noexcept -> bool;
    auto init_if(const string_view option_name) noexcept -> bool;
    auto update() noexcept -> work_done;
    void finish() noexcept;

private:
    std::optional<router> _router;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

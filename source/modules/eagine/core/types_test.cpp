/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///

#include <eagine/testing/unit_begin_ctx.hpp>
import std;
import eagine.core;
import eagine.msgbus.core;
//------------------------------------------------------------------------------
// priority increase decrease
//------------------------------------------------------------------------------
void message_priority_inc_dec(auto& s) {
    using P = eagine::msgbus::message_priority;
    eagitest::case_ test{s, 1, "priority inc/dec"};

    test.check(P::normal < increased(P::normal), "1");
    test.check(decreased(P::normal) < P::normal, "1");
    test.check(increased(decreased(P::normal)) == P::normal, "3");
    test.check(decreased(increased(P::normal)) == P::normal, "4");
    test.check(P::idle == decreased(P::idle), "5");
    test.check(P::critical == increased(P::critical), "6");
}
//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
auto test_main(eagine::test_ctx& ctx) -> int {
    eagitest::ctx_suite test{ctx, "types", 1};
    test.once(message_priority_inc_dec);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end_ctx.hpp>

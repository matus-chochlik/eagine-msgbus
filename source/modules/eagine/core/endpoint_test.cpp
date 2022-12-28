/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#include <eagine/testing/unit_begin_ctx.hpp>
import eagine.core;
import eagine.msgbus.core;
import <chrono>;
//------------------------------------------------------------------------------
// preconfigure  id
//------------------------------------------------------------------------------
void endpoint_preconfigure_id(unsigned r, auto& s) {
    eagitest::case_ test{s, 1, "preconfigure id"};
    auto& ctx{s.context()};

    eagine::msgbus::endpoint endpoint_a{"EndpointA", ctx};
    eagine::msgbus::endpoint endpoint_b{"EndpointB", ctx};
    eagine::msgbus::endpoint endpoint_c{"EndpointC", ctx};

    const auto id_ofs{eagine::identifier_t{r}};
    endpoint_a.preconfigure_id(11 + id_ofs);
    endpoint_b.preconfigure_id(17 + id_ofs);
    endpoint_c.preconfigure_id(23 + id_ofs);

    auto acceptor = eagine::msgbus::make_direct_acceptor(ctx);
    endpoint_a.add_connection(acceptor->make_connection());
    endpoint_b.add_connection(acceptor->make_connection());
    endpoint_c.add_connection(acceptor->make_connection());

    eagine::msgbus::router router(ctx);
    router.add_acceptor(std::move(acceptor));

    eagine::timeout get_id_time{std::chrono::seconds{5}};

    while(not(
      endpoint_a.has_id() and endpoint_b.has_id() and endpoint_c.has_id())) {
        if(r % 2 == 0) {
            router.update();
            endpoint_a.update();
            endpoint_b.update();
            endpoint_c.update();
        } else {
            endpoint_a.update();
            endpoint_b.update();
            endpoint_c.update();
            router.update();
        }
        if(get_id_time.is_expired()) {
            test.fail("too late");
            break;
        }
    }

    test.check_equal(endpoint_a.get_id(), 11 + id_ofs, "id a");
    test.check_equal(endpoint_b.get_id(), 17 + id_ofs, "id b");
    test.check_equal(endpoint_c.get_id(), 23 + id_ofs, "id c");
}
//------------------------------------------------------------------------------
// get id
//------------------------------------------------------------------------------
void endpoint_get_id(unsigned r, auto& s) {
    eagitest::case_ test{s, 2, "get id"};
    auto& ctx{s.context()};

    eagine::msgbus::endpoint endpoint_a{"EndpointA", ctx};
    eagine::msgbus::endpoint endpoint_b{"EndpointB", ctx};
    eagine::msgbus::endpoint endpoint_c{"EndpointC", ctx};

    auto acceptor = eagine::msgbus::make_direct_acceptor(ctx);
    endpoint_a.add_connection(acceptor->make_connection());
    endpoint_b.add_connection(acceptor->make_connection());
    endpoint_c.add_connection(acceptor->make_connection());

    eagine::msgbus::router router(ctx);
    router.add_acceptor(std::move(acceptor));

    eagine::timeout get_id_time{std::chrono::seconds{5}};

    while(not(
      endpoint_a.has_id() and endpoint_b.has_id() and endpoint_c.has_id())) {
        if(r % 2 == 0) {
            router.update();
            endpoint_a.update();
            endpoint_b.update();
            endpoint_c.update();
        } else {
            endpoint_a.update();
            endpoint_b.update();
            endpoint_c.update();
            router.update();
        }
        if(get_id_time.is_expired()) {
            test.fail("too late");
            break;
        }
    }

    test.check(endpoint_a.get_id() != endpoint_b.get_id(), "different ids ab");
    test.check(endpoint_b.get_id() != endpoint_c.get_id(), "different ids bc");
    test.check(endpoint_c.get_id() != endpoint_a.get_id(), "different ids ca");
}
//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
auto test_main(eagine::test_ctx& ctx) -> int {
    enable_message_bus(ctx);
    ctx.preinitialize();

    eagitest::ctx_suite test{ctx, "endpoint", 2};
    test.repeat(10, endpoint_preconfigure_id);
    test.repeat(10, endpoint_get_id);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end_ctx.hpp>

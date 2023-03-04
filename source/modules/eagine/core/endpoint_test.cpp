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
import std;
//------------------------------------------------------------------------------
// connection established signal
//------------------------------------------------------------------------------
void endpoint_connection_established(unsigned, auto& s) {
    eagitest::case_ test{s, 1, "connection established"};
    eagitest::track trck{test, 0, 3};
    auto& ctx{s.context()};

    eagine::msgbus::endpoint endpoint_a{"EndpointA", ctx};
    eagine::msgbus::endpoint endpoint_b{"EndpointB", ctx};
    eagine::msgbus::endpoint endpoint_c{"EndpointC", ctx};

    bool est_a{false};
    const auto slot_a = [&](bool has_id) {
        test.check_equal(has_id, endpoint_a.has_id(), "a has id");
        trck.checkpoint(1);
        est_a = true;
    };
    endpoint_a.connection_established.connect({eagine::construct_from, slot_a});

    bool est_b{false};
    const auto slot_b = [&](bool has_id) {
        test.check_equal(has_id, endpoint_b.has_id(), "b has id");
        trck.checkpoint(2);
        est_b = true;
    };
    endpoint_b.connection_established.connect({eagine::construct_from, slot_b});

    bool est_c{false};
    const auto slot_c = [&](bool has_id) {
        test.check_equal(has_id, endpoint_c.has_id(), "c has id");
        trck.checkpoint(3);
        est_c = true;
    };
    endpoint_c.connection_established.connect({eagine::construct_from, slot_c});

    auto acceptor = eagine::msgbus::make_direct_acceptor(ctx);
    endpoint_a.add_connection(acceptor->make_connection());
    endpoint_b.add_connection(acceptor->make_connection());
    endpoint_c.add_connection(acceptor->make_connection());

    eagine::msgbus::router router(ctx);
    router.add_acceptor(std::move(acceptor));

    eagine::timeout connect_time{std::chrono::seconds{3}};
    while(not(est_a and est_b and est_c)) {
        if(connect_time.is_expired()) {
            test.fail("too late");
            break;
        }
        router.update();
        endpoint_a.update();
        endpoint_b.update();
        endpoint_c.update();
    }
}
//------------------------------------------------------------------------------
// connection lost signal
//------------------------------------------------------------------------------
void endpoint_connection_lost(unsigned, auto& s) {
    eagitest::case_ test{s, 2, "connection lost"};
    eagitest::track trck{test, 0, 4};
    auto& ctx{s.context()};

    eagine::msgbus::endpoint endpoint_a{"EndpointA", ctx};
    eagine::msgbus::endpoint endpoint_b{"EndpointB", ctx};
    eagine::msgbus::endpoint endpoint_c{"EndpointC", ctx};

    bool est_a{false};
    bool lst_a{false};
    const auto established_a = [&](bool) {
        est_a = true;
    };
    endpoint_a.connection_established.connect(
      {eagine::construct_from, established_a});
    const auto lost_a = [&]() {
        lst_a = true;
        trck.checkpoint(2);
    };
    endpoint_a.connection_lost.connect({eagine::construct_from, lost_a});

    bool est_b{false};
    bool lst_b{false};
    const auto established_b = [&](bool) {
        est_b = true;
    };
    endpoint_b.connection_established.connect(
      {eagine::construct_from, established_b});
    const auto lost_b = [&]() {
        lst_b = true;
        trck.checkpoint(3);
    };
    endpoint_b.connection_lost.connect({eagine::construct_from, lost_b});

    bool est_c{false};
    bool lst_c{false};
    const auto established_c = [&](bool) {
        est_c = true;
    };
    endpoint_c.connection_established.connect(
      {eagine::construct_from, established_c});
    const auto lost_c = [&]() {
        lst_c = true;
        trck.checkpoint(4);
    };
    endpoint_c.connection_lost.connect({eagine::construct_from, lost_c});

    auto acceptor = eagine::msgbus::make_direct_acceptor(ctx);
    endpoint_a.add_connection(acceptor->make_connection());
    endpoint_b.add_connection(acceptor->make_connection());
    endpoint_c.add_connection(acceptor->make_connection());

    eagine::timeout connect_time{std::chrono::seconds{3}};
    if(not connect_time.is_expired()) {
        eagine::msgbus::router router(ctx);
        router.add_acceptor(std::move(acceptor));

        while(not(est_a and est_b and est_c)) {
            if(connect_time.is_expired()) {
                test.fail("failed to connect");
                break;
            }
            router.update();
            endpoint_a.update();
            endpoint_b.update();
            endpoint_c.update();
        }
        trck.checkpoint(1);
    }
    eagine::timeout loose_time{std::chrono::seconds{3}};
    while(not(lst_a and lst_b and lst_c)) {
        if(loose_time.is_expired()) {
            test.fail("failed to disconnect");
            break;
        }
        endpoint_a.update();
        endpoint_b.update();
        endpoint_c.update();
    }
}
//------------------------------------------------------------------------------
// preconfigure  id
//------------------------------------------------------------------------------
void endpoint_preconfigure_id(unsigned r, auto& s) {
    eagitest::case_ test{s, 3, "preconfigure id"};
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
            test.fail("failed to confirm id");
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
    eagitest::case_ test{s, 4, "get id"};
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
            test.fail("failed to get id");
            break;
        }
    }

    test.check(endpoint_a.get_id() != endpoint_b.get_id(), "different ids ab");
    test.check(endpoint_b.get_id() != endpoint_c.get_id(), "different ids bc");
    test.check(endpoint_c.get_id() != endpoint_a.get_id(), "different ids ca");
}
//------------------------------------------------------------------------------
// is assigned signal
//------------------------------------------------------------------------------
void endpoint_id_assigned(unsigned, auto& s) {
    eagitest::case_ test{s, 5, "id assigned"};
    eagitest::track trck{test, 0, 3};
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

    bool has_a{false};
    const auto slot_a = [&](bool has_id) {
        test.check_equal(has_id, endpoint_a.has_id(), "a has id");
        trck.checkpoint(1);
        has_a = true;
    };
    endpoint_a.connection_established.connect({eagine::construct_from, slot_a});

    bool has_b{false};
    const auto slot_b = [&](bool has_id) {
        test.check_equal(has_id, endpoint_b.has_id(), "b has id");
        trck.checkpoint(2);
        has_b = true;
    };
    endpoint_b.connection_established.connect({eagine::construct_from, slot_b});

    bool has_c{false};
    const auto slot_c = [&](bool has_id) {
        test.check_equal(has_id, endpoint_c.has_id(), "c has id");
        trck.checkpoint(3);
        has_c = true;
    };
    endpoint_c.connection_established.connect({eagine::construct_from, slot_c});

    eagine::timeout get_id_time{std::chrono::seconds{5}};
    while(not(has_a and has_b and has_c)) {
        if(get_id_time.is_expired()) {
            test.fail("failed to get id");
            break;
        }
        router.update();
        endpoint_a.update();
        endpoint_b.update();
        endpoint_c.update();
    }
}
//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
auto test_main(eagine::test_ctx& ctx) -> int {
    enable_message_bus(ctx);
    ctx.preinitialize();

    eagitest::ctx_suite test{ctx, "endpoint", 5};
    test.repeat(5, endpoint_connection_established);
    test.repeat(5, endpoint_connection_lost);
    test.repeat(5, endpoint_preconfigure_id);
    test.repeat(5, endpoint_get_id);
    test.repeat(5, endpoint_id_assigned);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end_ctx.hpp>

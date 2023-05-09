/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#include <eagine/testing/unit_begin_ctx.hpp>
import std;
import eagine.core;
import eagine.msgbus.core;
import eagine.msgbus.services;
//------------------------------------------------------------------------------
// test 1
//------------------------------------------------------------------------------
void ping_pong_1(auto& s) {
    eagitest::case_ test{s, 1, "1"};
    eagitest::track trck{test, 0, 2};
    auto& ctx{s.context()};

    eagine::msgbus::endpoint ping_ept{"PingEndpt", ctx};
    eagine::msgbus::endpoint pong_ept{"PongEndpt", ctx};

    auto acceptor = eagine::msgbus::make_direct_acceptor(ctx);
    ping_ept.add_connection(acceptor->make_connection());
    pong_ept.add_connection(acceptor->make_connection());

    eagine::msgbus::router router(ctx);
    router.add_acceptor(std::move(acceptor));

    eagine::msgbus::service_composition<eagine::msgbus::pinger<>> pinger{
      ping_ept};
    eagine::msgbus::service_composition<eagine::msgbus::pingable<>> pingable{
      pong_ept};

    while(not(ping_ept.has_id() and pong_ept.has_id())) {
        router.update();
        pinger.update();
        pingable.update();
        pinger.process_one();
        pingable.process_one();
    }

    const auto pingable_ept_id{pong_ept.get_id()};
    eagine::timeout ping_time{std::chrono::milliseconds{100}};
    eagine::msgbus::message_sequence_t prev_seq_no{0};
    int todo{100};

    const auto handle_responded{[&](
                                  const eagine::msgbus::result_context&,
                                  const eagine::msgbus::ping_response& pong) {
        test.check_equal(pong.pingable_id, pingable_ept_id, "pingable id ok");
        if(pong.pingable_id == pingable_ept_id) {
            --todo;
            trck.checkpoint(1);
        }
        if(prev_seq_no > 0) {
            test.check(prev_seq_no < pong.sequence_no, "sequence ok");
            trck.checkpoint(2);
        }
        prev_seq_no = pong.sequence_no;
        test.check(pong.age < ping_time.period(), "age ok");
    }};
    pinger.ping_responded.connect({eagine::construct_from, handle_responded});

    const auto handle_timeouted{[&](const eagine::msgbus::ping_timeout& to) {
        if(to.pingable_id == pingable_ept_id) {
            test.fail("ping timeouted");
        }
    }};
    pinger.ping_timeouted.connect({eagine::construct_from, handle_timeouted});

    while(todo > 0) {
        router.update();
        pinger.update();
        pingable.update();
        pinger.process_one();
        pingable.process_one();
        pinger.ping_if(pingable_ept_id, ping_time);
    }
}
//------------------------------------------------------------------------------
// test 2
//------------------------------------------------------------------------------
void ping_pong_2(auto& s) {
    eagitest::case_ test{s, 2, "2"};
    eagitest::track trck{test, 0, 2};
    auto& ctx{s.context()};

    eagine::msgbus::endpoint ping_ept{"PingEndpt", ctx};
    eagine::msgbus::endpoint pong_ept{"PongEndpt", ctx};

    auto acceptor = eagine::msgbus::make_direct_acceptor(ctx);
    pong_ept.add_connection(acceptor->make_connection());
    ping_ept.add_connection(acceptor->make_connection());

    eagine::msgbus::router router(ctx);
    router.add_acceptor(std::move(acceptor));

    eagine::msgbus::service_composition<eagine::msgbus::pinger<>> pinger{
      ping_ept};
    eagine::msgbus::service_composition<eagine::msgbus::pingable<>> pingable{
      pong_ept};

    while(not(ping_ept.has_id() and pong_ept.has_id())) {
        router.update();
        pinger.update();
        pingable.update();
        pingable.process_all();
        pinger.process_all();
    }

    const auto pingable_ept_id{pong_ept.get_id()};
    eagine::msgbus::message_sequence_t prev_seq_no{0};
    int todo{100};

    const auto handle_responded{[&](
                                  const eagine::msgbus::result_context&,
                                  const eagine::msgbus::ping_response& pong) {
        test.check_equal(pong.pingable_id, pingable_ept_id, "pingable id ok");
        if(pong.pingable_id == pingable_ept_id) {
            --todo;
            trck.checkpoint(1);
        }
        if(prev_seq_no > 0) {
            test.check(prev_seq_no < pong.sequence_no, "sequence ok");
            trck.checkpoint(2);
        }
        prev_seq_no = pong.sequence_no;
    }};
    pinger.ping_responded.connect({eagine::construct_from, handle_responded});

    const auto handle_timeouted{[&](const eagine::msgbus::ping_timeout& fail) {
        if(fail.pingable_id == pingable_ept_id) {
            test.fail("ping timeouted");
        }
    }};
    pinger.ping_timeouted.connect({eagine::construct_from, handle_timeouted});

    while(todo > 0) {
        router.update();
        pinger.update();
        pingable.update();
        pingable.process_all();
        pinger.process_all();
        pinger.ping(pingable_ept_id, std::chrono::milliseconds{100});
    }
}
//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
auto test_main(eagine::test_ctx& ctx) -> int {
    enable_message_bus(ctx);
    ctx.preinitialize();

    eagitest::ctx_suite test{ctx, "ping-pong", 2};
    test.once(ping_pong_1);
    test.once(ping_pong_2);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end_ctx.hpp>

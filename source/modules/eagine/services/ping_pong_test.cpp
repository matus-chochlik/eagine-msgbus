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
import eagine.msgbus.services;
import <chrono>;
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

    pinger.ping_responded.connect(
      {eagine::construct_from,
       [&](
         const eagine::identifier_t pingable_id,
         const eagine::msgbus::message_sequence_t seq_no,
         const std::chrono::microseconds age,
         const eagine::msgbus::verification_bits) {
           test.check_equal(pingable_id, pingable_ept_id, "pingable id ok");
           if(pingable_id == pingable_ept_id) {
               --todo;
               trck.passed_part(1);
           }
           if(prev_seq_no > 0) {
               test.check(prev_seq_no < seq_no, "sequence ok");
               trck.passed_part(2);
           }
           prev_seq_no = seq_no;
           test.check(age < ping_time.period(), "age ok");
       }});

    pinger.ping_timeouted.connect(
      {eagine::construct_from,
       [&](
         const eagine::identifier_t pingable_id,
         const eagine::msgbus::message_sequence_t,
         const std::chrono::microseconds) {
           if(pingable_id == pingable_ept_id) {
               test.fail("ping timeouted");
           }
       }});

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
// main
//------------------------------------------------------------------------------
auto test_main(eagine::test_ctx& ctx) -> int {
    enable_message_bus(ctx);
    ctx.preinitialize();

    eagitest::ctx_suite test{ctx, "ping-pong", 1};
    test.once(ping_pong_1);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end_ctx.hpp>

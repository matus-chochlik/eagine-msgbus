
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
import eagine.msgbus.services;
//------------------------------------------------------------------------------
// test 1
//------------------------------------------------------------------------------
void shutdown_1(auto& s) {
    eagitest::case_ test{s, 1, "1"};
    eagitest::track trck{test, 0, 2};
    auto& ctx{s.context()};

    eagine::msgbus::endpoint target_ept{"Target", ctx};
    eagine::msgbus::endpoint source_1_ept{"Source1", ctx};
    eagine::msgbus::endpoint source_2_ept{"Source2", ctx};

    auto acceptor = eagine::msgbus::make_direct_acceptor(ctx);
    target_ept.add_connection(acceptor->make_connection());
    source_1_ept.add_connection(acceptor->make_connection());
    source_2_ept.add_connection(acceptor->make_connection());

    eagine::msgbus::router router(ctx);
    router.add_acceptor(std::move(acceptor));

    eagine::msgbus::service_composition<eagine::msgbus::shutdown_target<>>
      target{target_ept};
    eagine::msgbus::service_composition<eagine::msgbus::shutdown_invoker<>>
      source_1{source_1_ept};
    eagine::msgbus::service_composition<eagine::msgbus::shutdown_invoker<>>
      source_2{source_2_ept};

    while(not(
      source_1_ept.has_id() and source_2_ept.has_id() and
      target_ept.has_id())) {
        router.update();
        source_1.update_and_process_all();
        source_2.update_and_process_all();
        target.update_and_process_all();
    }

    bool handled_1{false};
    bool handled_2{false};

    const auto handle_request{[&](
                                const eagine::msgbus::result_context&,
                                const eagine::msgbus::shutdown_request& req) {
        if(req.source_id == source_1_ept.get_id()) {
            handled_1 = true;
            trck.checkpoint(1);
        } else if(req.source_id == source_2_ept.get_id()) {
            handled_2 = true;
            trck.checkpoint(2);
        }
    }};
    target.shutdown_requested.connect({eagine::construct_from, handle_request});

    source_1.shutdown_one(target_ept.get_id());
    source_2.shutdown_one(target_ept.get_id());

    eagine::timeout shutdown_time{std::chrono::seconds{5}};
    while(not(handled_1 and handled_2)) {
        router.update();
        source_1.update_and_process_all();
        source_2.update_and_process_all();
        target.update_and_process_all();
        if(shutdown_time.is_expired()) {
            test.fail("failed to shutdown");
            break;
        }
    }
}
//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
auto test_main(eagine::test_ctx& ctx) -> int {
    enable_message_bus(ctx);
    ctx.preinitialize();

    eagitest::ctx_suite test{ctx, "shutdown", 1};
    test.once(shutdown_1);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end_ctx.hpp>

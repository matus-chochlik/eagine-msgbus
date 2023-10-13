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
import eagine.msgbus.utility;
//------------------------------------------------------------------------------
// test 1
//------------------------------------------------------------------------------
void resource_transfer_1(auto& s) {
    eagitest::case_ test{s, 1, "1"};
    eagitest::track trck{test, 0, 2};
    auto& ctx{s.context()};
    eagine::msgbus::registry the_reg{ctx};

    auto& server =
      the_reg.emplace<eagine::msgbus::resource_data_server_node>("Server");
    auto& consumer =
      the_reg.emplace<eagine::msgbus::resource_data_consumer_node>("Consumer");

    if(the_reg.wait_for_id_of(std::chrono::seconds{30}, server, consumer)) {
        eagine::span_size_t todo_zeroes{16777216};
        eagine::span_size_t todo_ones{16777216};
        eagine::span_size_t todo_all{5 * 16777216};

        const auto consume{[&](const eagine::blob_chunk& chunk) {
            for(const auto& blk : chunk.data) {
                for(auto b : blk) {
                    if(b == 0x00U) {
                        if(todo_zeroes > 0) {
                            --todo_zeroes;
                        }
                    } else if(b == 0x01U) {
                        if(todo_ones > 0) {
                            --todo_ones;
                        }
                    }
                    --todo_all;
                    trck.checkpoint(1);
                }
            }
        }};

        consumer.blob_stream_data_appended.connect(
          {eagine::construct_from, consume});

        const auto enqueue{
          [&](
            eagine::url res_locator,
            const eagine::msgbus::message_priority msg_priority,
            const bool chunks) {
              if(chunks) {
                  consumer.fetch_resource_chunks(
                    std::move(res_locator),
                    4 * 1024,
                    msg_priority,
                    std::chrono::minutes{5});
              } else {
                  consumer.stream_resource(
                    std::move(res_locator),
                    msg_priority,
                    std::chrono::minutes{5});
              }
          }};

        enqueue(
          eagine::url("eagires:///random?count=16777216"),
          eagine::msgbus::message_priority::idle,
          true);
        enqueue(
          eagine::url("eagires:///ones?count=16777216"),
          eagine::msgbus::message_priority::low,
          false);
        enqueue(
          eagine::url("eagires:///zeroes?count=16777216"),
          eagine::msgbus::message_priority::normal,
          true);
        enqueue(
          eagine::url("eagires:///random?count=16777216"),
          eagine::msgbus::message_priority::high,
          false);
        enqueue(
          eagine::url("eagires:///random?count=16777216"),
          eagine::msgbus::message_priority::critical,
          true);

        test.check(consumer.has_pending_resources(), "has pending");

        eagine::timeout transfer_time{std::chrono::minutes{1}};
        while(todo_all > 0 and consumer.has_pending_resources()) {
            if(transfer_time.is_expired()) {
                test.fail("data transfer timeout");
                break;
            }
            the_reg.update_and_process();
        }

        test.check(todo_zeroes <= 0, "zeroes transferred");
        test.check(todo_ones <= 0, "ones transferred");

        trck.checkpoint(2);
    } else {
        test.fail("get id observer");
    }

    the_reg.finish();
}
//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
auto test_main(eagine::test_ctx& ctx) -> int {
    enable_message_bus(ctx);
    ctx.preinitialize();

    eagitest::ctx_suite test{ctx, "resource transfer", 1};
    test.once(resource_transfer_1);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end_ctx.hpp>

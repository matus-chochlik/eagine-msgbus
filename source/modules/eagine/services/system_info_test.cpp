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
void system_info_1(auto& s) {
    eagitest::case_ test{s, 1, "1"};
    eagitest::track trck{test, 0, 5};
    auto& ctx{s.context()};
    eagine::msgbus::registry the_reg{ctx};

    auto& provider = the_reg.emplace<eagine::msgbus::service_composition<
      eagine::msgbus::system_info_provider<>>>("Provider");
    auto& consumer = the_reg.emplace<eagine::msgbus::service_composition<
      eagine::msgbus::system_info_consumer<>>>("Consumer");

    if(the_reg.wait_for_id_of(std::chrono::seconds{30}, provider, consumer)) {

        bool has_uptime{false};
        bool has_cpu_concurrent_threads{false};
        bool has_short_average_load{false};
        bool has_long_average_load{false};
        bool has_memory_page_size{false};

        const auto received_all{[&] {
            return has_uptime and has_cpu_concurrent_threads and
                   has_short_average_load and has_long_average_load and
                   has_memory_page_size;
        }};

        // uptime
        const auto handle_uptime{[&](
                                   const eagine::msgbus::result_context& rc,
                                   const std::chrono::duration<float>& value) {
            has_uptime = value.count() > 0.F;
            test.check(provider.get_id() == rc.source_id(), "from provider");
            trck.checkpoint(1);
        }};
        consumer.uptime_received.connect(
          {eagine::construct_from, handle_uptime});

        // cpu concurrent threads
        const auto handle_cpu_concurrent_threads{
          [&](
            const eagine::msgbus::result_context& rc,
            const eagine::valid_if_positive<eagine::span_size_t>& value) {
              has_cpu_concurrent_threads = value.has_value();
              test.check(provider.get_id() == rc.source_id(), "from provider");
              trck.checkpoint(2);
          }};
        consumer.cpu_concurrent_threads_received.connect(
          {eagine::construct_from, handle_cpu_concurrent_threads});

        // short average load
        const auto handle_short_average_load{
          [&](
            const eagine::msgbus::result_context& rc,
            const eagine::valid_if_nonnegative<float>& value) {
              has_short_average_load = value.has_value();
              test.check(provider.get_id() == rc.source_id(), "from provider");
              trck.checkpoint(3);
          }};
        consumer.short_average_load_received.connect(
          {eagine::construct_from, handle_short_average_load});

        // long average load
        const auto handle_long_average_load{
          [&](
            const eagine::msgbus::result_context& rc,
            const eagine::valid_if_nonnegative<float>& value) {
              has_long_average_load = value.has_value();
              test.check(provider.get_id() == rc.source_id(), "from provider");
              trck.checkpoint(4);
          }};
        consumer.long_average_load_received.connect(
          {eagine::construct_from, handle_long_average_load});

        // memory page size
        const auto handle_memory_page_size{
          [&](
            const eagine::msgbus::result_context& rc,
            const eagine::valid_if_positive<eagine::span_size_t>& value) {
              has_memory_page_size = value.has_value();
              test.check(provider.get_id() == rc.source_id(), "from provider");
              trck.checkpoint(5);
          }};
        consumer.memory_page_size_received.connect(
          {eagine::construct_from, handle_memory_page_size});

        // test
        eagine::timeout query_timeout{std::chrono::seconds{5}, eagine::nothing};
        eagine::timeout receive_timeout{std::chrono::seconds{30}};
        while(not received_all()) {
            if(query_timeout.is_expired()) {
                if(not has_uptime) {
                    consumer.query_uptime(provider.get_id().value());
                }
                if(not has_cpu_concurrent_threads) {
                    consumer.query_cpu_concurrent_threads(
                      provider.get_id().value());
                }
                if(not has_short_average_load) {
                    consumer.query_short_average_load(
                      provider.get_id().value());
                }
                if(not has_long_average_load) {
                    consumer.query_long_average_load(provider.get_id().value());
                }
                if(not has_memory_page_size) {
                    consumer.query_memory_page_size(provider.get_id().value());
                }
                query_timeout.reset();
            }
            if(receive_timeout.is_expired()) {
                test.fail("receive timeout");
                break;
            }
            the_reg.update_all();
        }
    }

    the_reg.finish();
}
//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
auto test_main(eagine::test_ctx& ctx) -> int {
    enable_message_bus(ctx);
    ctx.preinitialize();

    eagitest::ctx_suite test{ctx, "endpoint info", 1};
    test.once(system_info_1);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end_ctx.hpp>

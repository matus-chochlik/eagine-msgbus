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
import std;
//------------------------------------------------------------------------------
// test 1
//------------------------------------------------------------------------------
void common_info_1(auto& s) {
    eagitest::case_ test{s, 1, "1"};
    eagitest::track trck{test, 0, 5};
    auto& ctx{s.context()};
    eagine::msgbus::registry the_reg{ctx};

    auto& provider = the_reg.emplace<eagine::msgbus::service_composition<
      eagine::msgbus::common_info_providers<>>>("Provider");
    auto& consumer = the_reg.emplace<eagine::msgbus::service_composition<
      eagine::msgbus::common_info_consumers<>>>("Consumer");

    if(the_reg.wait_for_id_of(std::chrono::seconds{30}, provider, consumer)) {
        provider.provided_endpoint_info().display_name = "test provider";
        provider.provided_endpoint_info().description = "test description";

        bool has_compiler_info{false};
        bool has_build_version_info{false};
        bool has_host_info{false};
        bool has_application_info{false};
        bool has_endpoint_info{false};

        const auto received_all{[&] {
            return has_compiler_info and has_build_version_info and
                   has_host_info and has_application_info and has_endpoint_info;
        }};

        // compiler info
        const auto handle_compiler_info{
          [&](
            const eagine::msgbus::result_context& rc,
            const eagine::compiler_info& info) {
              has_compiler_info =
                info.name().has_value() or info.architecture_name().has_value();
              test.check(provider.get_id() == rc.source_id(), "from provider");
              trck.checkpoint(1);
          }};

        consumer.compiler_info_received.connect(
          {eagine::construct_from, handle_compiler_info});

        // build info
        const auto handle_build_version_info{
          [&](
            const eagine::msgbus::result_context& rc,
            const eagine::version_info& info) {
              has_build_version_info = info.has_value();
              test.check(provider.get_id() == rc.source_id(), "from provider");
              trck.checkpoint(2);
          }};

        consumer.build_version_info_received.connect(
          {eagine::construct_from, handle_build_version_info});

        // host info
        const auto handle_host_info{
          [&](
            const eagine::msgbus::result_context& rc,
            const eagine::valid_if_not_empty<std::string>& name) {
              has_host_info = name.has_value();
              test.check(provider.get_id() == rc.source_id(), "from provider");
              trck.checkpoint(3);
          }};

        consumer.hostname_received.connect(
          {eagine::construct_from, handle_host_info});

        // application info
        const auto handle_application_info{
          [&](
            const eagine::msgbus::result_context& rc,
            const eagine::valid_if_not_empty<std::string>& name) {
              has_application_info = name.has_value();
              test.check(provider.get_id() == rc.source_id(), "from provider");
              trck.checkpoint(4);
          }};

        consumer.application_name_received.connect(
          {eagine::construct_from, handle_application_info});

        // endpoint info
        const auto handle_endpoint_info{
          [&](
            const eagine::msgbus::result_context& rc,
            const eagine::msgbus::endpoint_info& info) {
              has_endpoint_info = info.display_name == "test provider" and
                                  info.description == "test description";
              test.check(provider.get_id() == rc.source_id(), "from provider");
              trck.checkpoint(5);
          }};

        consumer.endpoint_info_received.connect(
          {eagine::construct_from, handle_endpoint_info});

        // test
        eagine::timeout query_timeout{std::chrono::seconds{5}, eagine::nothing};
        eagine::timeout receive_timeout{std::chrono::seconds{30}};
        while(not received_all()) {
            if(query_timeout.is_expired()) {
                if(not has_compiler_info) {
                    consumer.query_compiler_info(provider.get_id().value());
                }
                if(not has_build_version_info) {
                    consumer.query_build_version_info(
                      provider.get_id().value());
                }
                if(not has_host_info) {
                    consumer.query_hostname(provider.get_id().value());
                }
                if(not has_application_info) {
                    consumer.query_application_name(provider.get_id().value());
                }
                if(not has_endpoint_info) {
                    consumer.query_endpoint_info(provider.get_id().value());
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

    eagitest::ctx_suite test{ctx, "common info", 1};
    test.once(common_info_1);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end_ctx.hpp>

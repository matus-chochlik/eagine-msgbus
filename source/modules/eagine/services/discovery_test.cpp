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
template <typename Base = eagine::msgbus::subscriber>
class test_pong : public Base {

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();
        Base::add_method(
          this,
          eagine::msgbus::
            message_map<"eagiTest", "ping", &test_pong::_handle_ping>{});
    }

private:
    auto _handle_ping(
      const eagine::msgbus::message_context&,
      const eagine::msgbus::stored_message& message) noexcept -> bool {
        this->bus_node().respond_to(
          message, eagine::message_id{"eagiTest", "pong"}, {});
        return true;
    }
};
//------------------------------------------------------------------------------
template <typename Base = eagine::msgbus::subscriber>
class test_ping : public Base {
public:
    void assign_target(eagine::endpoint_id_t id) noexcept {
        _target = id;
    }

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();
        Base::add_method(
          this,
          eagine::msgbus::
            message_map<"eagiTest", "pong", &test_ping::_handle_pong>{});
    }

    auto update() -> eagine::work_done {
        eagine::some_true something_done{Base::update()};
        if(eagine::msgbus::is_valid_endpoint_id(_target)) {
            if(_ping_time.is_expired()) {
                eagine::msgbus::message_view ping_msg;
                ping_msg.set_target_id(_target);
                ping_msg.set_sequence_no(_seq_id);
                this->bus_node().post({"eagiTest", "ping"}, ping_msg);
                _ping_time.reset();
                something_done();
            }
        }
        return something_done;
    }

private:
    auto _handle_pong(
      const eagine::msgbus::message_context&,
      const eagine::msgbus::stored_message&) noexcept -> bool {
        ++_rcvd;
        return true;
    }

    int _rcvd{0};
    eagine::msgbus::message_sequence_t _seq_id{0};
    eagine::timeout _ping_time{std::chrono::milliseconds{1}};
    eagine::endpoint_id_t _target{};
};
//------------------------------------------------------------------------------
// test 1
//------------------------------------------------------------------------------
void discovery_1(auto& s) {
    eagitest::case_ test{s, 1, "1"};
    eagitest::track trck{test, 0, 2};
    auto& ctx{s.context()};
    eagine::msgbus::registry the_reg{ctx};

    auto& observer = the_reg.emplace<eagine::msgbus::service_composition<
      eagine::msgbus::subscriber_discovery<>>>("Observer");

    if(the_reg.wait_for_id_of(std::chrono::seconds{30}, observer)) {
        auto& pinger =
          the_reg.emplace<eagine::msgbus::service_composition<test_ping<>>>(
            "TestPing");
        auto& ponger =
          the_reg.emplace<eagine::msgbus::service_composition<test_pong<>>>(
            "TestPong");

        bool found_pinger{false};
        bool found_ponger{false};
        bool pinger_alive{false};
        bool ponger_alive{false};

        const auto discovered_all{[&] {
            return found_pinger and found_ponger and pinger_alive and
                   ponger_alive;
        }};

        const auto handle_alive{
          [&](
            const eagine::msgbus::result_context&,
            const eagine::msgbus::subscriber_alive& alive) {
              if(pinger.get_id() == alive.source.endpoint_id) {
                  pinger_alive = true;
              }
              if(ponger.get_id() == alive.source.endpoint_id) {
                  ponger_alive = true;
              }
              trck.checkpoint(1);
          }};
        observer.reported_alive.connect({eagine::construct_from, handle_alive});

        const auto handle_subscribed{
          [&](
            const eagine::msgbus::result_context&,
            const eagine::msgbus::subscriber_subscribed& sub) {
              if(sub.message_type.is("eagiTest", "pong")) {
                  test.check_equal(
                    sub.source.endpoint_id,
                    pinger.get_id().value_or(
                      eagine::msgbus::invalid_endpoint_id()),
                    "pinger id");
                  found_pinger = true;
              } else if(sub.message_type.is("eagiTest", "ping")) {
                  test.check_equal(
                    sub.source.endpoint_id,
                    ponger.get_id().value_or(
                      eagine::msgbus::invalid_endpoint_id()),
                    "ponger id");
                  found_ponger = true;
              }
              trck.checkpoint(2);
          }};
        observer.subscribed.connect(
          {eagine::construct_from, handle_subscribed});

        if(the_reg.wait_for_id_of(std::chrono::seconds{30}, pinger, ponger)) {
            pinger.assign_target(ponger.bus_node().get_id());
            eagine::timeout discovery_time{std::chrono::minutes{1}};
            while(not discovered_all()) {
                if(discovery_time.is_expired()) {
                    test.fail("discovery timeout");
                    break;
                }
                the_reg.update_and_process();
            }
        } else {
            test.fail("get id ping/pong");
        }

        test.check(found_pinger, "found pinger");
        test.check(found_ponger, "found ponger");
        test.check(pinger_alive, "pinger alive");
        test.check(ponger_alive, "ponger alive");
    } else {
        test.fail("get id observer");
    }

    the_reg.finish();
}
//------------------------------------------------------------------------------
// test 2
//------------------------------------------------------------------------------
void discovery_2(auto& s) {
    eagitest::case_ test{s, 2, "2"};
    eagitest::track trck{test, 0, 3};
    auto& ctx{s.context()};
    eagine::msgbus::registry the_reg{ctx};

    auto& observer = the_reg.emplace<eagine::msgbus::service_composition<
      eagine::msgbus::subscriber_discovery<>>>("Observer");

    if(the_reg.wait_for_id_of(std::chrono::seconds{30}, observer)) {
        auto& pinger =
          the_reg.emplace<eagine::msgbus::service_composition<test_ping<>>>(
            "TestPing");
        auto& ponger =
          the_reg.emplace<eagine::msgbus::service_composition<test_pong<>>>(
            "TestPong");

        bool found_pinger{false};
        bool found_ponger{false};
        bool pinger_alive{false};
        bool ponger_alive{false};

        const auto discovered_all{[&] {
            return found_pinger and found_ponger and pinger_alive and
                   ponger_alive;
        }};

        pinger.assign_target(ponger.bus_node().get_id());
        eagine::timeout discovery_time{std::chrono::minutes{1}};

        const auto handler{eagine::overloaded(
          [](const std::monostate&) {},
          [](const eagine::msgbus::subscriber_unsubscribed&) {},
          [](const eagine::msgbus::subscriber_not_subscribed&) {},
          [&](const eagine::msgbus::subscriber_alive& alive) {
              if(pinger.get_id() == alive.source.endpoint_id) {
                  pinger_alive = true;
              }
              if(ponger.get_id() == alive.source.endpoint_id) {
                  ponger_alive = true;
              }
              trck.checkpoint(1);
          },
          [&](const eagine::msgbus::subscriber_subscribed& sub) {
              if(sub.message_type.is("eagiTest", "pong")) {
                  test.check_equal(
                    sub.source.endpoint_id,
                    pinger.get_id().value_or(
                      eagine::msgbus::invalid_endpoint_id()),
                    "pinger id");
                  found_pinger = true;
              }
              if(sub.message_type.is("eagiTest", "ping")) {
                  test.check_equal(
                    sub.source.endpoint_id,
                    ponger.get_id().value_or(
                      eagine::msgbus::invalid_endpoint_id()),
                    "ponger id");
                  found_ponger = true;
              }
              trck.checkpoint(2);
          })};

        while(not discovered_all()) {
            if(discovery_time.is_expired()) {
                test.fail("discovery timeout");
                break;
            }
            the_reg.update_only();

            for(auto& service : the_reg.services()) {
                for(auto& queue : service.process_queues()) {
                    for(auto& message : queue.give_messages()) {
                        std::visit(
                          handler, observer.decode(queue.context(), message));
                        trck.checkpoint(3);
                    }
                }
            }
        }

        test.check(found_pinger, "found pinger");
        test.check(found_ponger, "found ponger");
        test.check(pinger_alive, "pinger alive");
        test.check(ponger_alive, "ponger alive");
    } else {
        test.fail("get id observer");
    }

    the_reg.finish();
}
//------------------------------------------------------------------------------
// test 3
//------------------------------------------------------------------------------
void discovery_3(auto& s) {
    eagitest::case_ test{s, 3, "3"};
    eagitest::track trck{test, 0, 2};
    auto& ctx{s.context()};
    eagine::msgbus::registry the_reg{ctx};

    auto& observer = the_reg.emplace<eagine::msgbus::service_composition<
      eagine::msgbus::subscriber_discovery<>>>("Observer");

    if(the_reg.wait_for_id_of(std::chrono::seconds{30}, observer)) {
        auto& pinger =
          the_reg.emplace<eagine::msgbus::service_composition<test_ping<>>>(
            "TestPing");
        auto& ponger =
          the_reg.emplace<eagine::msgbus::service_composition<test_pong<>>>(
            "TestPong");

        bool found_pinger{false};
        bool found_ponger{false};
        bool pinger_alive{false};
        bool ponger_alive{false};

        const auto discovered_all{[&] {
            return found_pinger and found_ponger and pinger_alive and
                   ponger_alive;
        }};

        pinger.assign_target(ponger.bus_node().get_id());
        eagine::timeout discovery_time{std::chrono::minutes{1}};

        const auto handler{eagine::overloaded(
          [](const std::monostate&) {},
          [](const eagine::msgbus::subscriber_unsubscribed&) {},
          [](const eagine::msgbus::subscriber_not_subscribed&) {},
          [&](const eagine::msgbus::subscriber_alive& alive) {
              if(pinger.get_id() == alive.source.endpoint_id) {
                  pinger_alive = true;
              }
              if(ponger.get_id() == alive.source.endpoint_id) {
                  ponger_alive = true;
              }
              trck.checkpoint(1);
          },
          [&](const eagine::msgbus::subscriber_subscribed& sub) {
              if(sub.message_type.is("eagiTest", "pong")) {
                  test.check_equal(
                    sub.source.endpoint_id,
                    pinger.get_id().value_or(
                      eagine::msgbus::invalid_endpoint_id()),
                    "pinger id");
                  found_pinger = true;
              }
              if(sub.message_type.is("eagiTest", "ping")) {
                  test.check_equal(
                    sub.source.endpoint_id,
                    ponger.get_id().value_or(
                      eagine::msgbus::invalid_endpoint_id()),
                    "ponger id");
                  found_ponger = true;
              }
              trck.checkpoint(2);
          })};

        while(not discovered_all()) {
            if(discovery_time.is_expired()) {
                test.fail("discovery timeout");
                break;
            }
            the_reg.update_only();

            for(auto&& [response, decoded] : observer.give_decoded()) {
                (void)response;
                std::visit(handler, decoded);
            }
            pinger.process_all();
            ponger.process_all();
        }

        test.check(found_pinger, "found pinger");
        test.check(found_ponger, "found ponger");
        test.check(pinger_alive, "pinger alive");
        test.check(ponger_alive, "ponger alive");
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

    eagitest::ctx_suite test{ctx, "discovery", 3};
    test.once(discovery_1);
    test.once(discovery_2);
    test.once(discovery_3);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end_ctx.hpp>

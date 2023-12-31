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
//------------------------------------------------------------------------------
template <typename Base = eagine::msgbus::subscriber>
class test_pong : public Base {
public:
    void assign(eagitest::track& trck) noexcept {
        _ptrck = &trck;
    }

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
        if(_ptrck) {
            _ptrck->checkpoint(1);
        }
        this->bus_node().respond_to(
          message, eagine::message_id{"eagiTest", "pong"}, {});
        return true;
    }

    eagitest::track* _ptrck{nullptr};
};
//------------------------------------------------------------------------------
template <typename Base = eagine::msgbus::subscriber>
class test_ping : public Base {
public:
    void assign(eagitest::track& trck) noexcept {
        _ptrck = &trck;
    }

    void assign_target(eagine::endpoint_id_t id) noexcept {
        _target = id;
    }

    auto success() const noexcept -> bool {
        return _rcvd >= _max;
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
        if(is_valid_id(_target)) {
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
        if(_ptrck) {
            _ptrck->checkpoint(2);
        }
        ++_rcvd;
        return true;
    }

    int _max{5000};
    int _sent{0};
    int _rcvd{0};
    eagine::msgbus::message_sequence_t _seq_id{0};
    eagine::timeout _ping_time{std::chrono::milliseconds{1}};
    eagine::endpoint_id_t _target{};
    eagitest::track* _ptrck{nullptr};
};
//------------------------------------------------------------------------------
// get-id 1
//------------------------------------------------------------------------------
void registry_get_id_1(auto& s) {
    eagitest::case_ test{s, 1, "get-id 1"};
    eagitest::track trck{test, 0, 1};
    auto& ctx{s.context()};
    eagine::msgbus::registry the_reg{ctx};

    using pinger_t = eagine::msgbus::service_composition<
      eagine::msgbus::require_services<eagine::msgbus::subscriber, test_ping>>;
    using ponger_t = eagine::msgbus::service_composition<
      eagine::msgbus::require_services<eagine::msgbus::subscriber, test_pong>>;

    std::vector<std::reference_wrapper<pinger_t>> pingers;
    std::vector<std::reference_wrapper<ponger_t>> pongers;

    for(unsigned i = 0; i < test.repeats(100); ++i) {
        pingers.emplace_back(
          the_reg.emplace<pinger_t>(eagine::random_identifier()));
        pongers.emplace_back(
          the_reg.emplace<ponger_t>(eagine::random_identifier()));
    }

    const auto ids_assigned = [&]() -> bool {
        for(const auto& p : pingers) {
            if(not p.get().has_id()) {
                return false;
            }
        }
        for(const auto& p : pongers) {
            if(not p.get().has_id()) {
                return false;
            }
        }
        return true;
    };

    eagine::timeout get_id_time{std::chrono::minutes{1}};
    while(not ids_assigned()) {
        if(get_id_time.is_expired()) {
            test.fail("get-id timeout");
            break;
        }
        the_reg.update_and_process().or_sleep_for(std::chrono::milliseconds(1));
        trck.checkpoint(1);
    }

    the_reg.finish();
}
//------------------------------------------------------------------------------
// get-id 2
//------------------------------------------------------------------------------
void registry_get_id_2(auto& s) {
    eagitest::case_ test{s, 2, "get-id 2"};
    eagitest::track trck{test, 0, 1};
    auto& ctx{s.context()};
    eagine::msgbus::registry the_reg{ctx};

    auto& ponger = the_reg.emplace<eagine::msgbus::service_composition<
      eagine::msgbus::require_services<eagine::msgbus::subscriber, test_pong>>>(
      "TestPong");
    auto& pinger = the_reg.emplace<eagine::msgbus::service_composition<
      eagine::msgbus::require_services<eagine::msgbus::subscriber, test_ping>>>(
      "TestPing");

    if(the_reg.wait_for_id_of(std::chrono::minutes{1}, pinger, ponger)) {
        trck.checkpoint(1);
    } else {
        test.fail("get-id timeout");
    }

    the_reg.finish();
}
//------------------------------------------------------------------------------
// get-id 3
//------------------------------------------------------------------------------
void registry_get_id_3(auto& s) {
    eagitest::case_ test{s, 3, "get-id 3"};
    eagitest::track trck{test, 0, 1};
    auto& ctx{s.context()};
    eagine::msgbus::registry the_reg{ctx};

    the_reg.emplace<eagine::msgbus::service_composition<
      eagine::msgbus::require_services<eagine::msgbus::subscriber, test_pong>>>(
      "TestPong");
    the_reg.emplace<eagine::msgbus::service_composition<
      eagine::msgbus::require_services<eagine::msgbus::subscriber, test_ping>>>(
      "TestPing");

    if(the_reg.wait_for_ids(std::chrono::minutes{1})) {
        trck.checkpoint(1);
    } else {
        test.fail("get-id timeout");
    }

    the_reg.finish();
}
//------------------------------------------------------------------------------
// ping/pong 1
//------------------------------------------------------------------------------
void registry_ping_pong(auto& s) {
    eagitest::case_ test{s, 4, "ping-pong"};
    eagitest::track trck{test, 0, 4};
    auto& ctx{s.context()};
    eagine::msgbus::registry the_reg{ctx};

    auto& ponger = the_reg.emplace<eagine::msgbus::service_composition<
      eagine::msgbus::require_services<eagine::msgbus::subscriber, test_pong>>>(
      "TestPong");
    auto& pinger = the_reg.emplace<eagine::msgbus::service_composition<
      eagine::msgbus::require_services<eagine::msgbus::subscriber, test_ping>>>(
      "TestPing");

    ponger.assign(trck);
    pinger.assign(trck);

    eagine::timeout get_id_time{std::chrono::minutes{1}};
    while(not(ponger.has_id() and pinger.has_id())) {
        if(get_id_time.is_expired()) {
            test.fail("get-id timeout");
            break;
        }
        the_reg.update_and_process().or_sleep_for(std::chrono::milliseconds(1));
        trck.checkpoint(3);
    }

    if(ponger.has_id()) {
        pinger.assign_target(ponger.bus_node().get_id());

        eagine::timeout ping_time{std::chrono::minutes{1}};
        while(not pinger.success()) {
            if(ping_time.is_expired()) {
                test.fail("ping timeout");
                break;
            }
            the_reg.update_and_process().or_sleep_for(
              std::chrono::milliseconds(1));
            trck.checkpoint(4);
        }
    }
    the_reg.finish();
}
//------------------------------------------------------------------------------
// ping/pong 2
//------------------------------------------------------------------------------
void registry_wait_ping_pong(auto& s) {
    eagitest::case_ test{s, 5, "wait / ping-pong"};
    eagitest::track trck{test, 0, 3};
    auto& ctx{s.context()};
    eagine::msgbus::registry the_reg{ctx};

    auto& ponger = the_reg.emplace<eagine::msgbus::service_composition<
      eagine::msgbus::require_services<eagine::msgbus::subscriber, test_pong>>>(
      "TestPong");
    auto& pinger = the_reg.emplace<eagine::msgbus::service_composition<
      eagine::msgbus::require_services<eagine::msgbus::subscriber, test_ping>>>(
      "TestPing");

    ponger.assign(trck);
    pinger.assign(trck);

    if(the_reg.wait_for_id_of(std::chrono::minutes{1}, pinger, ponger)) {
        pinger.assign_target(ponger.bus_node().get_id());

        eagine::timeout ping_time{std::chrono::minutes{1}};
        while(not pinger.success()) {
            if(ping_time.is_expired()) {
                test.fail("ping timeout");
                break;
            }
            the_reg.update_and_process().or_sleep_for(
              std::chrono::milliseconds(1));
            trck.checkpoint(3);
        }
    }
    the_reg.finish();
}
//------------------------------------------------------------------------------
// queues
//------------------------------------------------------------------------------
void registry_queues(auto& s) {
    eagitest::case_ test{s, 6, "queues"};
    eagitest::track trck{test, 0, 8};
    auto& ctx{s.context()};
    eagine::msgbus::registry the_reg{ctx};

    auto& ponger = the_reg.emplace<eagine::msgbus::service_composition<
      eagine::msgbus::require_services<eagine::msgbus::subscriber, test_pong>>>(
      "TestPong");
    auto& pinger = the_reg.emplace<eagine::msgbus::service_composition<
      eagine::msgbus::require_services<eagine::msgbus::subscriber, test_ping>>>(
      "TestPing");

    ponger.assign(trck);
    pinger.assign(trck);

    if(not the_reg.wait_for_ids(std::chrono::minutes{1})) {
        test.fail("get-id timeout");
    } else {
        pinger.assign_target(ponger.bus_node().get_id());

        eagine::timeout ping_time{std::chrono::minutes{1}};
        while(not pinger.success()) {
            if(ping_time.is_expired()) {
                test.fail("ping timeout");
                break;
            }
            the_reg.update_only().or_sleep_for(std::chrono::milliseconds(1));

            for(auto& service : the_reg.services()) {
                for(auto& queue : service.process_queues()) {
                    for(auto& message : queue.give_messages()) {
                        test.check(
                          is_valid_id(message.source_id), "valid source id");
                        if(queue.context().msg_id().is("eagiTest", "ping")) {
                            test.check_equal(
                              message.source_id,
                              pinger.bus_node().get_id(),
                              "pinger id");
                            trck.checkpoint(3);
                        }
                        if(queue.context().msg_id().is("eagiTest", "pong")) {
                            test.check_equal(
                              message.source_id,
                              ponger.bus_node().get_id(),
                              "ponger id");
                            trck.checkpoint(4);
                        }
                        trck.checkpoint(5);
                    }
                    trck.checkpoint(6);
                }
                trck.checkpoint(7);
            }
            trck.checkpoint(8);
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

    eagitest::ctx_suite test{ctx, "registry", 6};
    test.once(registry_get_id_1);
    test.once(registry_get_id_2);
    test.once(registry_get_id_3);
    test.once(registry_ping_pong);
    test.once(registry_wait_ping_pong);
    test.once(registry_queues);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end_ctx.hpp>

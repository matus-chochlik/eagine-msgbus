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
void topology_1(auto& s) {
    eagitest::case_ test{s, 1, "1"};
    eagitest::track trck{test, 0, 2};
    auto& ctx{s.context()};
    eagine::msgbus::registry the_reg{ctx};

    auto& topology = the_reg.emplace<
      eagine::msgbus::service_composition<eagine::msgbus::network_topology<>>>(
      "Topology");

    bool found_router{false};
    bool found_pinger{false};
    bool found_ponger{false};

    const auto discovered_all{[&] {
        return found_router and found_pinger and found_ponger;
    }};

    const auto handle_router_appeared{
      [&](
        const eagine::msgbus::result_context&,
        const eagine::msgbus::router_topology_info& info) {
          if(info.router_id == the_reg.router_id()) {
              found_router = true;
          }
          trck.checkpoint(1);
      }};
    topology.router_appeared.connect(
      {eagine::construct_from, handle_router_appeared});

    auto& pinger =
      the_reg.emplace<eagine::msgbus::service_composition<test_ping<>>>(
        "TestPing");
    auto& ponger =
      the_reg.emplace<eagine::msgbus::service_composition<test_pong<>>>(
        "TestPong");

    const auto handle_endpoint_appeared{
      [&](
        const eagine::msgbus::result_context&,
        const eagine::msgbus::endpoint_topology_info& info) {
          if(pinger.get_id() == info.endpoint_id) {
              found_pinger = true;
          }
          if(ponger.get_id() == info.endpoint_id) {
              found_ponger = true;
          }
          trck.checkpoint(2);
      }};
    topology.endpoint_appeared.connect(
      {eagine::construct_from, handle_endpoint_appeared});

    if(the_reg.wait_for_id_of(std::chrono::seconds{30}, pinger, ponger)) {
        pinger.assign_target(ponger.bus_node().get_id());

        eagine::timeout discovery_time{std::chrono::minutes{1}};
        eagine::timeout query_time{std::chrono::seconds{10}, eagine::nothing};
        while(not discovered_all()) {
            if(query_time.is_expired()) {
                topology.discover_topology();
                query_time.reset();
            }
            if(discovery_time.is_expired()) {
                test.fail("discovery timeout");
                break;
            }
            the_reg.update_and_process();
        }
    } else {
        test.fail("get id ping/pong");
    }

    the_reg.finish();
}
//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
auto test_main(eagine::test_ctx& ctx) -> int {
    enable_message_bus(ctx);
    ctx.preinitialize();

    eagitest::ctx_suite test{ctx, "topology", 1};
    test.once(topology_1);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end_ctx.hpp>

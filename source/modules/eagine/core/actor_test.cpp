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
import <chrono>;
//------------------------------------------------------------------------------
class test_pong : public eagine::msgbus::actor<1> {
public:
    using base = eagine::msgbus::actor<1>;
    using base::bus_node;

    test_pong(
      eagine::main_ctx_parent parent,
      std::unique_ptr<eagine::msgbus::connection> conn,
      eagitest::track& trck)
      : base(
          {"TestPong", parent},
          this,
          eagine::msgbus::message_map<"PingPong", "Ping", &test_pong::ping>{})
      , _trck{trck} {
        this->add_connection(std::move(conn));
        this->allow_subscriptions();
    }

    auto ping(
      const eagine::msgbus::message_context&,
      const eagine::msgbus::stored_message& msg_in) noexcept -> bool {
        bus_node().respond_to(msg_in, {"PingPong", "Pong"});
        _timeout.reset();
        _trck.checkpoint(2);
        return true;
    }

    void update() noexcept {
        if(not _sent and _ready_timeout) {
            bus_node().broadcast({"PingPong", "Ready"});
            _ready_timeout.reset();
        }
        this->process_all();
    }

    auto failed() const noexcept {
        return _timeout.is_expired();
    }

private:
    eagitest::track& _trck;
    std::size_t _sent{0};
    eagine::timeout _timeout{std::chrono::minutes(1)};
    eagine::timeout _ready_timeout{std::chrono::seconds(1)};
};
//------------------------------------------------------------------------------
class test_ping : public eagine::msgbus::actor<2> {
public:
    using base = eagine::msgbus::actor<2>;
    using base::bus_node;

    test_ping(
      eagine::main_ctx_parent parent,
      std::unique_ptr<eagine::msgbus::connection> conn,
      eagitest::track& trck)
      : base(
          {"TestPing", parent},
          this,
          eagine::msgbus::message_map<"PingPong", "Pong", &test_ping::pong>{},
          eagine::msgbus::message_map<"PingPong", "Ready", &test_ping::ready>{})
      , _trck{trck}
      , _max{eagine::running_on_valgrind() ? 1000U : 50000U} {
        this->add_connection(std::move(conn));
        this->allow_subscriptions();
    }

    auto ready(
      const eagine::msgbus::message_context&,
      const eagine::msgbus::stored_message&) noexcept -> bool {
        _ready = true;
        _trck.checkpoint(3);
        return true;
    }

    auto pong(
      const eagine::msgbus::message_context&,
      const eagine::msgbus::stored_message&) noexcept -> bool {
        ++_rcvd;
        if(_rcvd < _max) {
            _timeout.reset();
            _trck.checkpoint(4);
        }
        return true;
    }

    void update() noexcept {
        if(_ready and (_sent <= _max * 2) and (_sent < _rcvd + _lmod)) {
            bus_node().broadcast({"PingPong", "Ping"});
        }
        this->process_all();
    }

    auto success() const noexcept -> bool {
        return _rcvd >= _max;
    }

    auto failed() const noexcept -> bool {
        return _timeout.is_expired();
    }

private:
    eagitest::track& _trck;
    std::size_t _lmod{1};
    std::size_t _sent{0};
    std::size_t _rcvd{0};
    const std::size_t _max;
    eagine::timeout _timeout{std::chrono::seconds(30)};
    bool _ready{false};
};
//------------------------------------------------------------------------------
void actor_ping_pong(auto& s) {
    auto& ctx{s.context()};
    eagitest::case_ test{s, 1, "ping-pong"};
    eagitest::track trck{test, 0, 4};

    auto fact{eagine::msgbus::make_direct_connection_factory(s.context())};
    test.ensure(bool(fact), "has factory");
    auto cacc{std::dynamic_pointer_cast<eagine::msgbus::direct_acceptor_intf>(
      std::shared_ptr<eagine::msgbus::acceptor>(
        fact->make_acceptor(eagine::identifier{"test"})))};
    test.ensure(bool(cacc), "has acceptor");

    auto ping_conn{cacc->make_connection()};
    test.ensure(bool(ping_conn), "has ping connection");
    test_ping ping(ctx, std::move(ping_conn), trck);

    auto pong_conn{cacc->make_connection()};
    test.ensure(bool(pong_conn), "has pong connection");
    test_pong pong(ctx, std::move(pong_conn), trck);

    eagine::msgbus::router router(ctx);
    router.add_acceptor(std::move(cacc));

    eagine::timeout ping_time{std::chrono::minutes{1}};
    while(not ping.success()) {
        if(ping_time.is_expired()) {
            test.fail("ping timeout");
            break;
        }
        if(ping.failed()) {
            test.fail("ping failed");
            break;
        }
        if(pong.failed()) {
            test.fail("pong failed");
            break;
        }
        router.update();
        ping.update();
        pong.update();
        trck.checkpoint(1);
    }
}
//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
auto test_main(eagine::test_ctx& ctx) -> int {
    eagitest::ctx_suite test{ctx, "actor", 1};
    test.once(actor_ping_pong);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end_ctx.hpp>

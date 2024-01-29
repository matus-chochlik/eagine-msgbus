/// @example eagine/msgbus/006_pong.cpp
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
import eagine.core;
import eagine.sslplus;
import eagine.msgbus;
import std;

namespace eagine {
namespace msgbus {
//------------------------------------------------------------------------------
class pong : public actor<2> {
public:
    using base = actor<2>;
    using base::bus_node;

    pong(
      main_ctx_parent parent,
      connection_setup& conn_setup,
      const string_view address)
      : base(
          {"ExamplPong", parent},
          this,
          message_map<"PingPong", "Ping", &pong::ping>{},
          message_map<"PingPong", "Shutdown", &pong::shutdown>{})
      , _lmod{running_on_valgrind() ? 1000U : 10000U} {
        this->allow_subscriptions();
        conn_setup.setup_connectors(
          *this,
          connection_kind::local_interprocess |
            connection_kind::remote_interprocess,
          address);
    }

    auto ping(const message_context&, const stored_message& msg_in) noexcept
      -> bool {
        bus_node().respond_to(msg_in, {"PingPong", "Pong"});
        if(++_sent % _lmod == 0) {
            bus_node().log_info("sent ${count} pongs").arg("count", _sent);
        }
        _timeout.reset();
        return true;
    }

    auto shutdown(const message_context&, const stored_message&) noexcept
      -> bool {
        _done = true;
        bus_node().log_info("received shutdown message");
        return true;
    }

    void update() noexcept {
        if(not _sent and _ready_timeout) {
            bus_node().broadcast({"PingPong", "Ready"});
            _ready_timeout.reset();
        } else {
            std::this_thread::yield();
        }
    }

    auto is_done() const noexcept {
        return _done or _timeout;
    }

private:
    std::size_t _lmod{1};
    std::size_t _sent{0};
    timeout _timeout{std::chrono::minutes(1)};
    timeout _ready_timeout{std::chrono::seconds(1)};
    bool _done{false};
};
//------------------------------------------------------------------------------
} // namespace msgbus

auto main(main_ctx& ctx) -> int {
    msgbus::router_address address{ctx};
    msgbus::connection_setup conn_setup(ctx);

    msgbus::pong pong(ctx, conn_setup, address);

    while(not pong.is_done()) {
        pong.process_all();
        pong.update();
    }

    return 0;
}
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    return eagine::default_main(argc, argv, eagine::main);
}


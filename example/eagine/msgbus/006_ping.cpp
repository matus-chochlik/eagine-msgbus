/// @example eagine/msgbus/006_ping.cpp
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
import eagine.core;
import eagine.sslplus;
import eagine.msgbus;
import <thread>;

namespace eagine {
namespace msgbus {
//------------------------------------------------------------------------------
class ping : public actor<2> {
public:
    using base = actor<2>;
    using base::bus_node;

    ping(
      main_ctx_parent parent,
      connection_setup& conn_setup,
      const string_view address,
      const valid_if_positive<std::size_t>& max)
      : base(
          {"ExamplPing", parent},
          this,
          message_map<"PingPong", "Pong", &ping::pong>{},
          message_map<"PingPong", "Ready", &ping::ready>{})
      , _lmod{running_on_valgrind() ? 1000U : 10000U}
      , _max{extract_or(max, running_on_valgrind() ? 10000U : 100000U)} {
        this->allow_subscriptions();

        conn_setup.setup_connectors(
          *this,
          connection_kind::local_interprocess |
            connection_kind::remote_interprocess,
          address);
    }

    auto pong(const message_context&, const stored_message&) noexcept -> bool {
        if(++_rcvd % _lmod == 0) {
            bus_node().log_info("received ${count} pongs").arg("count", _rcvd);
        }
        if(_rcvd < _max) {
            _timeout.reset();
        }
        return true;
    }

    auto ready(const message_context&, const stored_message&) noexcept -> bool {
        _ready = true;
        bus_node().log_info("received pong ready message");
        return true;
    }

    void shutdown() noexcept {
        bus_node().broadcast({"PingPong", "Shutdown"});
        bus_node().log_info("sent shutdown message");
    }

    void update() noexcept {
        if(_ready and (_sent <= _max * 2) and (_sent < _rcvd + _lmod)) {
            bus_node().broadcast({"PingPong", "Ping"});
            if(++_sent % _lmod == 0) {
                bus_node().log_info("sent ${count} pings").arg("count", _sent);
            }
        } else {
            std::this_thread::yield();
        }
    }

    auto is_done() const noexcept -> bool {
        return (_rcvd >= _max) or _timeout;
    }

    auto pings_per_second(const std::chrono::duration<float> s) const noexcept {
        return float(_rcvd) / s.count();
    }

private:
    std::size_t _lmod{1};
    std::size_t _sent{0};
    std::size_t _rcvd{0};
    const std::size_t _max{1000000};
    timeout _timeout{std::chrono::seconds(30)};
    bool _ready{false};
};
//------------------------------------------------------------------------------
} // namespace msgbus

auto main(main_ctx& ctx) -> int {
    msgbus::router_address address{ctx};
    msgbus::connection_setup conn_setup(ctx);

    valid_if_positive<std::size_t> ping_count{};
    if(auto arg{ctx.args().find("--ping-count")}) {
        arg.next().parse(ping_count, ctx.log().error_stream());
    }

    msgbus::ping ping(ctx, conn_setup, address, ping_count);

    const time_measure run_time;

    while(not ping.is_done()) {
        ping.update();
        ping.process_all();
    }

    const auto elapsed = run_time.seconds();

    ctx.log()
      .info("execution time ${time}, ${pps} pings per second")
      .arg("time", elapsed)
      .arg("pps", ping.pings_per_second(elapsed));

    ping.shutdown();

    return 0;
}
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    return eagine::default_main(argc, argv, eagine::main);
}


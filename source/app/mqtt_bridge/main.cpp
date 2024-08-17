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
//------------------------------------------------------------------------------
auto handle_special_args(main_ctx& ctx) {
    return handle_common_special_args(ctx);
}
//------------------------------------------------------------------------------
namespace msgbus {
using bridge_node_base = service_composition<
  require_services<subscriber, shutdown_target, pingable, common_info_providers>>;
//------------------------------------------------------------------------------
class bridge_node
  : public main_ctx_object
  , public bridge_node_base {
    using base = bridge_node_base;

public:
    bridge_node(endpoint& bus)
      : main_ctx_object{"MQTBrgNode", bus}
      , base{bus} {
        declare_state("running", "brdgStart", "brdgFinish");

        if(_shutdown_ignore) {
            log_info("shutdown requests are ignored due to configuration");
        } else {
            if(_shutdown_verify) {
                log_info("shutdown verification is enabled");
            } else {
                log_info("shutdown verification is disabled");
            }
            log_info("shutdown delay is set to ${delay}")
              .arg("delay", _shutdown_timeout.period());

            connect<&bridge_node::on_shutdown>(this, shutdown_requested);
        }
        auto& info = provided_endpoint_info();
        info.display_name = "bridge control node";
        info.description =
          "endpoint monitoring and controlling a message bus MQTT bridge";
        info.is_bridge_node = true;
    }

    static void active_state(const logger& log) {
        log.active_state("BridgeNode", "running");
    }

    void log_start() {
        log_change("message bus MQTT bridge started").tag("brdgStart");
    }

    void log_finish() {
        log_change("message bus MQTT bridge finishing").tag("brdgFinish");
    }

    auto update() -> work_done {
        return base::update_and_process_all();
    }

    auto is_shut_down() const noexcept {
        return _do_shutdown and _shutdown_timeout;
    }

private:
    timeout _shutdown_timeout{
      cfg_init("msgbus.bridge.shutdown.delay", std::chrono::seconds(30))};
    const std::chrono::milliseconds _shutdown_max_age{cfg_init(
      "msgbus.bridge.shutdown.max_age",
      std::chrono::milliseconds(2500))};
    const bool _shutdown_ignore{cfg_init("msgbus.bridge.keep_running", false)};
    const bool _shutdown_verify{
      cfg_init("msgbus.bridge.shutdown.verify", true)};
    bool _do_shutdown{false};

    auto _shutdown_verified(const verification_bits v) const noexcept -> bool {
        return v.has_all(
          verification_bit::source_id,
          verification_bit::source_certificate,
          verification_bit::source_private_key,
          verification_bit::message_id);
    }

    void on_shutdown(
      const result_context&,
      const shutdown_request& req) noexcept {
        log_info("received ${age} old shutdown request from ${source}")
          .arg("age", req.age)
          .arg("source", req.source_id)
          .arg("verified", req.verified);

        if(not _shutdown_ignore) {
            if(req.age <= _shutdown_max_age) {
                if(not _shutdown_verify or _shutdown_verified(req.verified)) {
                    log_info("request is valid, shutting down");
                    _do_shutdown = true;
                    _shutdown_timeout.reset();
                } else {
                    log_warning("shutdown verification failed");
                }
            } else {
                log_warning("shutdown request is too old");
            }
        } else {
            log_warning("ignoring shutdown request due to configuration");
        }
    }
};
} // namespace msgbus
//------------------------------------------------------------------------------
auto main(main_ctx& ctx) -> int {
    if(const auto exit_code{handle_special_args(ctx)}) {
        return *exit_code;
    }

    signal_switch interrupted;
    const auto& log = ctx.log();
    const auto sig_bind{log.log_when_switched(interrupted)};

    msgbus::bridge_node::active_state(log);

    enable_message_bus(ctx);

    log.info("message bus MQTT bridge starting up");

    ctx.system().preinitialize();

    msgbus::mqtt_bridge bridge(ctx);
    bridge.add_ca_certificate_pem(ca_certificate_pem(ctx));
    bridge.add_certificate_pem(msgbus::bridge_certificate_pem(ctx));
    msgbus::setup_connectors(ctx, bridge);

    std::uintmax_t cycles_work{0};
    std::uintmax_t cycles_idle{0};
    int idle_streak{0};
    int max_idle_streak{0};

    msgbus::endpoint node_endpoint{"BrdgNodeEp", ctx};
    node_endpoint.add_ca_certificate_pem(ca_certificate_pem(ctx));
    msgbus::setup_connectors(ctx, node_endpoint);
    {
        msgbus::bridge_node node{node_endpoint};

        auto alive{ctx.watchdog().start_watch()};
        node.log_start();

        while(not(interrupted or node.is_shut_down() or bridge.is_done()))
          [[likely]] {
            some_true something_done{};
            something_done(bridge.update());
            something_done(node.update());

            if(something_done) {
                ++cycles_work;
                idle_streak = 0;
            } else {
                ++cycles_idle;
                max_idle_streak = math::maximum(max_idle_streak, ++idle_streak);
                std::this_thread::sleep_for(
                  std::chrono::microseconds(math::minimum(idle_streak, 8000)));
            }
            alive.notify();
        }
        node.log_finish();
    }
    bridge.finish();

    log.stat("message bus MQTT bridge finishing")
      .arg("working", cycles_work)
      .arg("idling", cycles_idle)
      .arg(
        "workRate",
        "Ratio",
        float(cycles_work) / (float(cycles_idle) + float(cycles_work)))
      .arg(
        "idleRate",
        "Ratio",
        float(cycles_idle) / (float(cycles_idle) + float(cycles_work)))
      .arg("maxIdlStrk", max_idle_streak);

    return 0;
}
//------------------------------------------------------------------------------
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    eagine::main_ctx_options options;
    options.app_id = "BridgeExe";
    return eagine::main_impl(argc, argv, options, &eagine::main);
}

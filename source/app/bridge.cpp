///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

import eagine.core;
import eagine.sslplus;
import eagine.msgbus;
import std;

namespace eagine {
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
      : main_ctx_object{"BridgeNode", bus}
      , base{bus} {
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
          "endpoint monitoring and controlling a message bus bridge";
        info.is_bridge_node = true;
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

    void on_shutdown(const shutdown_request& req) noexcept {
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
    const signal_switch interrupted;
    enable_message_bus(ctx);

    auto& log = ctx.log();

    log.info("message bus bridge starting up");

    ctx.system().preinitialize();

    msgbus::bridge bridge(ctx);
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

        auto& wd = ctx.watchdog();
        wd.declare_initialized();

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
            wd.notify_alive();
        }
        wd.announce_shutdown();
    }
    bridge.finish();

    log.stat("message bus bridge finishing")
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
void maybe_start_coprocess(int& argc, const char**& argv);
auto maybe_cleanup(int result) -> int;
//------------------------------------------------------------------------------
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    eagine::maybe_start_coprocess(argc, argv);
    eagine::main_ctx_options options;
    options.app_id = "BridgeExe";
    return eagine::maybe_cleanup(
      eagine::main_impl(argc, argv, options, &eagine::main));
}

#if EAGINE_POSIX
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace eagine {
//------------------------------------------------------------------------------
#if EAGINE_POSIX
static ::pid_t ssh_coprocess_pid = -1;
#endif
//------------------------------------------------------------------------------
void maybe_start_coprocess(
  [[maybe_unused]] int& argc,
  [[maybe_unused]] const char**& argv) {
#if EAGINE_POSIX
    for(int argi = 1; argi < argc; ++argi) {
        program_arg arg{argi, argc, argv};
        if(arg.is_tag("--ssh")) {
            int pipe_b2c[2] = {-1, -1};
            int pipe_c2b[2] = {-1, -1};
            [[maybe_unused]] const int pipe_res_b2c =
              ::pipe(static_cast<int*>(pipe_b2c));
            [[maybe_unused]] const int pipe_res_c2b =
              ::pipe(static_cast<int*>(pipe_c2b));
            EAGINE_ASSERT(pipe_res_b2c == 0 and pipe_res_c2b == 0);

            const int fork_res = ::fork();
            EAGINE_ASSERT(fork_res >= 0);
            if(fork_res == 0) {
                if(const auto ssh_host{arg.next()}) {
                    ::close(pipe_b2c[1]);
                    ::close(0);
                    ::dup2(pipe_b2c[0], 0);

                    ::close(pipe_c2b[0]);
                    ::close(1);
                    ::dup2(pipe_c2b[1], 1);

                    const char* ssh_exe = std::getenv("EAGINE_SSH");
                    if(not ssh_exe) {
                        ssh_exe = "ssh";
                    }
                    ::execlp( // NOLINT(hicpp-vararg)
                      ssh_exe,
                      ssh_exe,
                      "-T",
                      "-e",
                      "none",
                      "-q",
                      "-o",
                      "BatchMode=yes",
                      c_str(ssh_host.get()).c_str(),
                      ".config/eagine/ssh-bridge",
                      "service_bridge",
                      nullptr);
                }
            } else {
                ::close(pipe_c2b[1]);
                ::close(0);
                ::dup2(pipe_c2b[0], 0);

                ::close(pipe_b2c[0]);
                ::close(1);
                ::dup2(pipe_b2c[1], 1);

                ssh_coprocess_pid = fork_res;
            }
        }
    }
#endif
}
//------------------------------------------------------------------------------
auto maybe_cleanup(int result) -> int {
#if EAGINE_POSIX
    if(ssh_coprocess_pid > 0) {
        int status = 0;
        ::kill(ssh_coprocess_pid, SIGTERM);
        ::waitpid(ssh_coprocess_pid, &status, 0);
        // NOLINTNEXTLINE(hicpp-signed-bitwise)
        if(not WIFEXITED(status) or WEXITSTATUS(status) != 0) {
            return status;
        }
    }
#endif
    return result;
}
//------------------------------------------------------------------------------
} // namespace eagine

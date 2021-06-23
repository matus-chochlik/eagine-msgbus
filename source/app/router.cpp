///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#include <eagine/logging/root_logger.hpp>
#include <eagine/main_ctx.hpp>
#include <eagine/main_fwd.hpp>
#include <eagine/math/functions.hpp>
#include <eagine/message_bus.hpp>
#include <eagine/msgbus/direct.hpp>
#include <eagine/msgbus/endpoint.hpp>
#include <eagine/msgbus/resources.hpp>
#include <eagine/msgbus/router.hpp>
#include <eagine/msgbus/service/common_info.hpp>
#include <eagine/msgbus/service/ping_pong.hpp>
#include <eagine/msgbus/service/shutdown.hpp>
#include <eagine/msgbus/service/system_info.hpp>
#include <eagine/signal_switch.hpp>
#include <eagine/sslplus/resources.hpp>
#include <eagine/watchdog.hpp>
#include <cstdint>

namespace eagine {
//------------------------------------------------------------------------------
namespace msgbus {
using router_node_base = service_composition<require_services<
  subscriber,
  shutdown_target,
  pingable,
  system_info_provider,
  common_info_providers>>;
//------------------------------------------------------------------------------
class router_node
  : public main_ctx_object
  , public router_node_base {
    using base = router_node_base;

public:
    router_node(endpoint& bus)
      : main_ctx_object{EAGINE_ID(RouterNode), bus}
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
              .arg(EAGINE_ID(delay), _shutdown_timeout.period());

            shutdown_requested.connect(EAGINE_THIS_MEM_FUNC_REF(on_shutdown));
        }
        auto& info = provided_endpoint_info();
        info.display_name = "router control node";
        info.description =
          "endpoint monitoring and controlling a message bus router";
        info.is_router_node = true;
    }

    auto update() -> work_done {
        return base::update_and_process_all();
    }

    auto is_shut_down() const noexcept {
        return _do_shutdown && _shutdown_timeout;
    }

private:
    timeout _shutdown_timeout{
      cfg_init("msgbus.router.shutdown.delay", std::chrono::seconds(60))};
    const std::chrono::milliseconds _shutdown_max_age{cfg_init(
      "msgbus.router.shutdown.max_age",
      std::chrono::milliseconds(2500))};
    const bool _shutdown_ignore{cfg_init("msgbus.router.keep_running", false)};
    const bool _shutdown_verify{
      cfg_init("msgbus.router.shutdown.verify", true)};
    bool _do_shutdown{false};

    auto _shutdown_verified(verification_bits v) const noexcept -> bool {
        return v.has_all(
          verification_bit::source_id,
          verification_bit::source_certificate,
          verification_bit::source_private_key,
          verification_bit::message_id);
    }

    void on_shutdown(
      std::chrono::milliseconds age,
      identifier_t source_id,
      verification_bits verified) {
        log_info("received ${age} old shutdown request from ${source}")
          .arg(EAGINE_ID(age), age)
          .arg(EAGINE_ID(source), source_id)
          .arg(EAGINE_ID(verified), verified);

        if(!_shutdown_ignore) {
            if(age <= _shutdown_max_age) {
                if(!_shutdown_verify || _shutdown_verified(verified)) {
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
    signal_switch interrupted;
    enable_message_bus(ctx);

    auto& log = ctx.log();
    log.info("message bus router starting up");

    ctx.system().preinitialize();

    auto local_acceptor{std::make_unique<msgbus::direct_acceptor>(ctx)};
    auto node_connection{local_acceptor->make_connection()};

    msgbus::router router(ctx);
    router.add_ca_certificate_pem(ca_certificate_pem(ctx));
    router.add_certificate_pem(msgbus::router_certificate_pem(ctx));
    ctx.bus().setup_acceptors(router);
    router.add_acceptor(std::move(local_acceptor));

    std::uintmax_t cycles_work{0};
    std::uintmax_t cycles_idle{0};
    int idle_streak{0};
    int max_idle_streak{0};

    msgbus::endpoint node_endpoint{EAGINE_ID(RutrNodeEp), ctx};
    node_endpoint.add_certificate_pem(msgbus::endpoint_certificate_pem(ctx));
    node_endpoint.add_connection(std::move(node_connection));
    {
        msgbus::router_node node{node_endpoint};

        auto& wd = ctx.watchdog();
        wd.declare_initialized();

        while(EAGINE_LIKELY(!(interrupted || node.is_shut_down()))) {
            some_true something_done{};
            something_done(router.update(8));
            something_done(node.update());

            if(something_done) {
                ++cycles_work;
                idle_streak = 0;
            } else {
                ++cycles_idle;
                max_idle_streak = math::maximum(max_idle_streak, ++idle_streak);
                std::this_thread::sleep_for(
                  std::chrono::milliseconds(math::minimum(idle_streak / 8, 8)));
            }
            wd.notify_alive();
        }
        wd.announce_shutdown();
    }

    router.finish();

    log.stat("message bus router finishing")
      .arg(EAGINE_ID(working), cycles_work)
      .arg(EAGINE_ID(idling), cycles_idle)
      .arg(
        EAGINE_ID(workRate),
        EAGINE_ID(Ratio),
        float(cycles_work) / (float(cycles_idle) + float(cycles_work)))
      .arg(
        EAGINE_ID(idleRate),
        EAGINE_ID(Ratio),
        float(cycles_idle) / (float(cycles_idle) + float(cycles_work)))
      .arg(EAGINE_ID(maxIdlStrk), max_idle_streak);

    return 0;
}
//------------------------------------------------------------------------------
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    eagine::main_ctx_options options;
    options.app_id = EAGINE_ID(RouterExe);
    return eagine::main_impl(argc, argv, options);
}

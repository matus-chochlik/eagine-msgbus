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
namespace msgbus {
//------------------------------------------------------------------------------
using pingable_base =
  require_services<subscriber, shutdown_target, pingable, common_info_providers>;

class pingable_node : public service_node<pingable_base> {
    using base = service_node<pingable_base>;

public:
    pingable_node(main_ctx_parent parent) noexcept
      : base{"PngablNode", parent} {
        connect<&pingable_node::on_shutdown>(this, shutdown_requested);
        auto& info = provided_endpoint_info();
        info.display_name = "pingable node";
        info.description = "simple generic pingable node";

        msgbus::setup_connectors(main_context(), *this);
    }

    auto respond_to_ping(
      const identifier_t,
      const message_sequence_t,
      const verification_bits) noexcept -> bool final {
        if((++_sent % _mod) == 0) [[unlikely]] {
            log_info("sent ${sent} pongs").arg("sent", _sent);
        }
        return true;
    }

    void on_shutdown(
      const result_context&,
      const shutdown_request& req) noexcept {
        log_info("received shutdown request from ${source}")
          .arg("age", req.age)
          .arg("source", req.source_id)
          .arg("verified", req.verified);

        _done = true;
    }

    auto is_done() const noexcept -> bool {
        return _done;
    }

    auto update() -> work_done {
        some_true something_done{};
        something_done(base::update());
        if(_sent < 1) {
            if(_announce_timeout) {
                this->announce_subscriptions();
                _announce_timeout.reset();
                something_done();
            }
        }
        return something_done;
    }

private:
    std::intmax_t _mod{10000};
    std::intmax_t _sent{0};
    timeout _announce_timeout{std::chrono::seconds(5)};
    bool _done{false};
};
//------------------------------------------------------------------------------
} // namespace msgbus

auto main(main_ctx& ctx) -> int {
    const auto& log{ctx.log()};
    log.active_state("ponging");
    log.declare_state("ponging", "pongStart", "pongFinish");

    enable_message_bus(ctx);
    ctx.preinitialize();

    msgbus::pingable_node the_pingable{main_ctx_object{"PngablEndp", ctx}};

    if(const auto id_arg{ctx.args().find("--pingable-id").next()}) {
        identifier_t id{0};
        if(assign_if_fits(id_arg, id)) {
            the_pingable.bus_node().preconfigure_id(id);
        }
    }

    log.change("starting").tag("pongStart");
    while(not the_pingable.is_done()) {
        the_pingable.update_and_process_all().or_sleep_for(
          std::chrono::milliseconds(1));
    }
    log.change("finished").tag("pongFinish");

    return 0;
}
//------------------------------------------------------------------------------
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    eagine::main_ctx_options options;
    options.app_id = "PongExe";
    return eagine::main_impl(argc, argv, options, &eagine::main);
}

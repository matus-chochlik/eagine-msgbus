///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#include <eagine/main_ctx.hpp>
#include <eagine/main_fwd.hpp>
#include <eagine/message_bus.hpp>
#include <eagine/msgbus/service.hpp>
#include <eagine/msgbus/service/common_info.hpp>
#include <eagine/msgbus/service/ping_pong.hpp>
#include <eagine/msgbus/service/shutdown.hpp>
#include <eagine/timeout.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <thread>

namespace eagine {
namespace msgbus {
//------------------------------------------------------------------------------
using pingable_base =
  require_services<subscriber, shutdown_target, pingable, common_info_providers>;

class pingable_node : public service_node<pingable_base> {
    using base = service_node<pingable_base>;

public:
    auto on_shutdown_slot() noexcept {
        return EAGINE_THIS_MEM_FUNC_REF(on_shutdown);
    }

    pingable_node(main_ctx_parent parent)
      : base{EAGINE_ID(PngablNode), parent} {
        shutdown_requested.connect(on_shutdown_slot());
        auto& info = provided_endpoint_info();
        info.display_name = "pingable node";
        info.description = "simple generic pingable node";

        bus().setup_connectors(*this);
    }

    auto respond_to_ping(
      const identifier_t,
      const message_sequence_t,
      const verification_bits) -> bool final {
        if(EAGINE_UNLIKELY((++_sent % _mod) == 0)) {
            log_info("sent ${sent} pongs").arg(EAGINE_ID(sent), _sent);
        }
        return true;
    }

    void on_shutdown(
      const std::chrono::milliseconds age,
      const identifier_t source_id,
      const verification_bits verified) noexcept {
        log_info("received shutdown request from ${source}")
          .arg(EAGINE_ID(age), age)
          .arg(EAGINE_ID(source), source_id)
          .arg(EAGINE_ID(verified), verified);

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
    enable_message_bus(ctx);
    ctx.preinitialize();

    msgbus::pingable_node the_pingable{
      main_ctx_object{EAGINE_ID(PngablEndp), ctx}};

    if(auto id_arg{ctx.args().find("--pingable-id").next()}) {
        identifier_t id{0};
        if(id_arg.parse(id, ctx.log().error_stream())) {
            the_pingable.bus_node().preconfigure_id(id);
        }
    }

    while(!the_pingable.is_done()) {
        if(!the_pingable.update_and_process_all()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    return 0;
}
//------------------------------------------------------------------------------
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    eagine::main_ctx_options options;
    options.app_id = EAGINE_ID(PongExe);
    return eagine::main_impl(argc, argv, options);
}

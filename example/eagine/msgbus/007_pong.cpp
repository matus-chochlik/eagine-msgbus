/// @example eagine/msgbus/007_pong.cpp
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
#include <eagine/msgbus/service/build_info.hpp>
#include <eagine/msgbus/service/host_info.hpp>
#include <eagine/msgbus/service/ping_pong.hpp>
#include <eagine/msgbus/service/shutdown.hpp>
#include <eagine/msgbus/service/system_info.hpp>
#include <eagine/msgbus/service_requirements.hpp>
#include <eagine/timeout.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <thread>

namespace eagine {
namespace msgbus {
//------------------------------------------------------------------------------
using pong_base = service_composition<require_services<
  subscriber,
  pingable,
  build_info_provider,
  system_info_provider,
  host_info_provider,
  shutdown_target>>;

class pong_example
  : public main_ctx_object
  , public pong_base {
    using base = pong_base;

public:
    pong_example(endpoint& bus)
      : main_ctx_object{EAGINE_ID(PongExampl), bus}
      , base{bus} {
        shutdown_requested.connect(EAGINE_THIS_MEM_FUNC_REF(on_shutdown));
    }

    auto respond_to_ping(
      const identifier_t,
      const message_sequence_t,
      const verification_bits) noexcept -> bool final {
        if((++_sent % _mod) == 0) [[unlikely]] {
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

    msgbus::endpoint bus{main_ctx_object{EAGINE_ID(PongEndpt), ctx}};

    if(auto id_arg{ctx.args().find("--pingable-id").next()}) {
        identifier_t id{0};
        if(id_arg.parse(id, ctx.log().error_stream())) {
            bus.preconfigure_id(id);
        }
    }

    msgbus::pong_example the_ponger{bus};
    ctx.bus().setup_connectors(the_ponger);

    while(!the_ponger.is_done()) {
        the_ponger.process_all();
        if(!the_ponger.update()) {
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

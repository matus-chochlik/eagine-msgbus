/// @example eagine/msgbus/008_pong_registry.cpp
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
import eagine.core;
import eagine.sslplus;
import eagine.msgbus;
import <algorithm>;
import <atomic>;
import <chrono>;
import <cstdint>;
import <thread>;

namespace eagine {
namespace msgbus {
//------------------------------------------------------------------------------
using pong_base = service_composition<
  require_services<subscriber, pingable, common_info_providers, shutdown_target>>;

class pong_example
  : public main_ctx_object
  , public pong_base {
    using base = pong_base;

public:
    pong_example(endpoint& bus)
      : main_ctx_object{"PongExampl", bus}
      , base{bus} {
        connect<&pong_example::on_shutdown>(this, shutdown_requested);
    }

    auto respond_to_ping(
      const identifier_t,
      const message_sequence_t,
      const verification_bits) noexcept -> bool final {
        if((++_sent % _mod) == 0) [[unlikely]] {
            _log.info("sent ${sent} pongs").arg("sent", _sent);
        }
        return true;
    }

    void on_shutdown(
      const std::chrono::milliseconds age,
      const identifier_t source_id,
      const verification_bits verified) noexcept {
        log_info("received shutdown request from ${source}")
          .arg("age", age)
          .arg("source", source_id)
          .arg("verified", verified);

        _done = true;
    }

    auto is_done() const noexcept -> bool {
        return _done;
    }

    auto update() -> work_done {
        some_true something_done{base::update()};
        if(_sent < 1) {
            if(_announce_timeout) {
                this->announce_subscriptions();
                something_done();
            }
        }
        return something_done;
    }

private:
    logger _log{};
    std::intmax_t _mod{10000};
    std::intmax_t _sent{0};
    resetting_timeout _announce_timeout{std::chrono::seconds(5)};
    bool _done{false};
};
//------------------------------------------------------------------------------
} // namespace msgbus

auto main(main_ctx& ctx) -> int {
    enable_message_bus(ctx);
    msgbus::registry the_reg{ctx};

    valid_if_positive<int> opt_ponger_count{};
    if(auto arg{ctx.args().find("--ponger-count")}) {
        arg.next().parse(opt_ponger_count, ctx.log().error_stream());
    }
    const auto ponger_count = extract_or(opt_ponger_count, 1);

    std::atomic<int> still_working(ponger_count);
    std::vector<std::thread> workers;
    workers.reserve(std_size(ponger_count));
    for(int p = 0; p < ponger_count; ++p) {
        auto& bus = the_reg.establish("PongEndpt");
        workers.emplace_back([&still_working, &bus]() {
            msgbus::pong_example ponger(bus);
            while(!ponger.is_done()) {
                ponger.process_all();
                if(!ponger.update()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            --still_working;
        });
    }

    while(still_working) {
        the_reg.update();
    }

    for(auto& worker : workers) {
        worker.join();
    }

    return 0;
}
//------------------------------------------------------------------------------
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    eagine::main_ctx_options options;
    options.app_id = "PongRegExe";
    return eagine::main_impl(argc, argv, options, eagine::main);
}

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
using sudoku_helper_base = service_composition<require_services<
  subscriber,
  shutdown_target,
  pingable,
  common_info_providers,
  sudoku_helper>>;

class sudoku_helper_node
  : public main_ctx_object
  , public sudoku_helper_base {
public:
    sudoku_helper_node(endpoint& bus)
      : main_ctx_object{"SudokuNode", bus}
      , sudoku_helper_base{bus} {
        shutdown_requested.connect(
          make_callable_ref<&sudoku_helper_node::on_shutdown>(this));

        auto& info = provided_endpoint_info();
        info.display_name = "sudoku helper";
        info.description = "helper node for the sudoku solver service";
    }

    auto is_shut_down() const noexcept -> bool {
        return _do_shutdown;
    }

private:
    void on_shutdown(
      const std::chrono::milliseconds age,
      const identifier_t source_id,
      const verification_bits verified) noexcept {
        log_info("received shutdown request from ${source}")
          .arg("age", age)
          .arg("source", source_id)
          .arg("verified", verified);

        _do_shutdown = true;
    }

    bool _do_shutdown{false};
};
//------------------------------------------------------------------------------
} // namespace msgbus

auto main(main_ctx& ctx) -> int {
    const signal_switch interrupted;
    enable_message_bus(ctx);
    ctx.preinitialize();

    msgbus::registry the_reg{ctx};

    auto shutdown_when_idle = false;
    ctx.config().fetch(
      "msgbus.sudoku.helper.shutdown_when_idle", shutdown_when_idle);

    auto max_idle_time = std::chrono::seconds(30);
    ctx.config().fetch("msgbus.sudoku.helper.max_idle_time", max_idle_time);

    auto helper_count = extract_or(
      ctx.config().get<span_size_t>("msgbus.sudoku.helper.count"),
      extract_or(ctx.system().cpu_concurrent_threads(), 4));

    std::mutex helper_mutex;
    std::condition_variable helper_cond;
    std::vector<std::thread> helpers;
    helpers.reserve(std_size(helper_count));

    std::atomic<span_size_t> remaining = helper_count + 1;

    auto helper_main = [&]() {
        std::unique_lock init_lock{helper_mutex};
        auto& helper_node =
          the_reg.emplace<msgbus::sudoku_helper_node>("SdkHlpEndp");
        remaining--;
        helper_cond.notify_all();
        init_lock.unlock();

        if(std::unique_lock latch_lock{helper_mutex}) {
            while(remaining > 0) {
                helper_cond.wait(latch_lock);
            }
        }

        int idle_streak = 0;
        auto keep_running = [&]() {
            if(idle_streak > 5) {
                std::unique_lock check_lock{helper_mutex};
                if(interrupted) {
                    return false;
                }
            }
            return not(
              helper_node.is_shut_down() or
              (shutdown_when_idle and
               (helper_node.idle_time() > max_idle_time)));
        };

        while(keep_running()) {
            if(helper_node.update_and_process_all()) {
                idle_streak = 0;
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(
                  math::minimum(++idle_streak, 100000)));
            }
        }
    };

    for(span_size_t i = 0; i < helper_count; ++i) {
        helpers.emplace_back([&]() {
            helper_main();
            std::unique_lock finish_lock{helper_mutex};
            helper_count--;
            helper_cond.notify_one();
        });
    }

    if(std::unique_lock latch_lock{helper_mutex}) {
        while(remaining > 1) {
            helper_cond.wait(latch_lock);
        }
    }

    std::unique_lock init_lock{helper_mutex};
    the_reg.update();
    remaining--;
    helper_cond.notify_all();
    init_lock.unlock();

    auto& wd = ctx.watchdog();
    wd.declare_initialized();

    int idle_streak = 0;
    while(not(interrupted or the_reg.is_done())) {
        if(the_reg.update()) {
            idle_streak = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } else {
            std::this_thread::sleep_for(
              std::chrono::milliseconds(math::minimum(++idle_streak, 100)));
        }

        wd.notify_alive();

        std::unique_lock break_lock{helper_mutex};
        if(helper_count <= 0) {
            break;
        }
    }
    wd.announce_shutdown();

    for(auto& helper : helpers) {
        helper.join();
        the_reg.update();
    }

    the_reg.finish();
    return 0;
}

} // namespace eagine
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    eagine::main_ctx_options options;
    options.app_id = "SudokuHlpr";
    return eagine::main_impl(argc, argv, options, &eagine::main);
}

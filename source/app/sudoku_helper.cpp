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
// helper node
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
    sudoku_helper_node(endpoint& bus);

    auto is_shut_down() const noexcept -> bool {
        return _do_shutdown;
    }

private:
    void on_shutdown(
      const result_context&,
      const shutdown_request& req) noexcept;

    bool _do_shutdown{false};
};
//------------------------------------------------------------------------------
sudoku_helper_node::sudoku_helper_node(endpoint& bus)
  : main_ctx_object{"SudokuNode", bus}
  , sudoku_helper_base{bus} {
    shutdown_requested.connect(
      make_callable_ref<&sudoku_helper_node::on_shutdown>(this));

    auto& info = provided_endpoint_info();
    info.display_name = "sudoku helper";
    info.description = "helper node for the sudoku solver service";
}
//------------------------------------------------------------------------------
void sudoku_helper_node::on_shutdown(
  const result_context&,
  const shutdown_request& req) noexcept {
    log_info("received shutdown request from ${source}")
      .arg("age", req.age)
      .arg("source", req.source_id)
      .arg("verified", req.verified);

    _do_shutdown = true;
}
//------------------------------------------------------------------------------
// helpers manager
//------------------------------------------------------------------------------
class sudoku_helpers : public main_ctx_object {
public:
    sudoku_helpers(main_ctx_parent parent);
    ~sudoku_helpers() noexcept;

    auto are_done() noexcept -> bool;
    auto update() -> work_done;

private:
    auto _make_node() -> auto&;
    void _wait_until_workers_ready() noexcept;
    void _helper_main() noexcept;

    signal_switch _interrupted;
    msgbus::registry _registry{*this};
    application_config_value<bool> _shutdown_when_idle{
      *this,
      "msgbus.sudoku.helper.shutdown_when_idle",
      false};
    application_config_value<std::chrono::seconds> _max_idle_time{
      *this,
      "msgbus.sudoku.helper.max_idle_time",
      std::chrono::seconds{30}};
    application_config_value<span_size_t> _helper_count{
      *this,
      "msgbus.sudoku.helper.count",
      main_context().system().cpu_concurrent_threads().value_or(4)};

    span_size_t _starting{_helper_count};

    std::mutex _helper_mutex;
    std::condition_variable _helper_cond;
    std::vector<std::thread> _helpers;
};
//------------------------------------------------------------------------------
sudoku_helpers::sudoku_helpers(main_ctx_parent parent)
  : main_ctx_object{"SdkuHelprs", parent} {
    _helpers.reserve(_helper_count);

    for(span_size_t i = 0; i < _helper_count; ++i) {
        _helpers.emplace_back([this]() { _helper_main(); });
    }

    _wait_until_workers_ready();
}
//------------------------------------------------------------------------------
sudoku_helpers::~sudoku_helpers() noexcept {
    for(auto& helper : _helpers) {
        helper.join();
        _registry.update_self();
    }

    _registry.finish();
}
//------------------------------------------------------------------------------
auto sudoku_helpers::are_done() noexcept -> bool {
    return _interrupted or _registry.is_done() or _helper_count == 0;
}
//------------------------------------------------------------------------------
auto sudoku_helpers::update() -> work_done {
    return _registry.update_self();
}
//------------------------------------------------------------------------------
auto sudoku_helpers::_make_node() -> auto& {
    const std::lock_guard init_lock{_helper_mutex};
    auto& node = _registry.emplace<msgbus::sudoku_helper_node>("SdkHlpEndp");
    _starting--;
    _helper_cond.notify_all();
    return node;
}
//------------------------------------------------------------------------------
void sudoku_helpers::_wait_until_workers_ready() noexcept {
    if(std::unique_lock latch_lock{_helper_mutex}) {
        while(_starting > 0) {
            _helper_cond.wait(latch_lock);
        }
    }
}
//------------------------------------------------------------------------------
void sudoku_helpers::_helper_main() noexcept {
    auto& helper_node = _make_node();
    _wait_until_workers_ready();

    int idle_streak = 0;

    const auto keep_running{[&, this]() {
        if(idle_streak > 5) {
            std::unique_lock check_lock{_helper_mutex};
            if(_interrupted) {
                return false;
            }
        }
        return not(
          helper_node.is_shut_down() or
          (_shutdown_when_idle and
           (helper_node.idle_time() > _max_idle_time.value())));
    }};

    while(keep_running()) {
        if(helper_node.update_and_process_all()) {
            idle_streak = 0;
        } else {
            std::this_thread::sleep_for(
              std::chrono::microseconds(math::minimum(++idle_streak, 100000)));
        }
    }
}
//------------------------------------------------------------------------------
} // namespace msgbus
//------------------------------------------------------------------------------
auto main(main_ctx& ctx) -> int {
    const auto& log = ctx.log();
    log.active_state("running");
    log.declare_state("running", "helpStart", "helpFinish");

    enable_message_bus(ctx);
    ctx.preinitialize();

    msgbus::sudoku_helpers helpers{ctx};
    auto alive{ctx.watchdog().start_watch()};

    int idle_streak = 0;
    log.change("starting").tag("helpStart");
    while(not(helpers.are_done())) {
        if(helpers.update()) {
            idle_streak = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } else {
            std::this_thread::sleep_for(
              std::chrono::milliseconds(math::minimum(++idle_streak, 100)));
        }

        alive.notify();
    }
    log.change("finished").tag("helpFinish");

    return 0;
}

} // namespace eagine
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    eagine::main_ctx_options options;
    options.app_id = "SudokuHlpr";
    return eagine::main_impl(argc, argv, options, &eagine::main);
}
//------------------------------------------------------------------------------

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
using sudoku_tiling_base =
  require_services<subscriber, pingable, common_info_providers, sudoku_tiling>;

class sudoku_tiling_node : public service_node<sudoku_tiling_base> {
public:
    sudoku_tiling_node(main_ctx_parent parent);

    static void active_state(const logger& log) noexcept;
    void log_start() noexcept;
    void log_finish() noexcept;

private:
    template <unsigned S>
    void _handle_generated(
      const eagine::endpoint_id_t,
      const sudoku_tiles<S>& tiles,
      const sudoku_solver_key&) noexcept;

    void _handle_board_timeout(const sudoku_board_timeout& info) noexcept;

    bool _block_cells{cfg_init("msgbus.sudoku.solver.block_cells", false)};
    bool _print_progress{
      cfg_init("msgbus.sudoku.solver.print_progress", false)};
    bool _print_incomplete{
      cfg_init("msgbus.sudoku.solver.print_incomplete", false)};

    span_size_t _suspend_count{0};
};
//------------------------------------------------------------------------------
sudoku_tiling_node::sudoku_tiling_node(main_ctx_parent parent)
  : service_node<sudoku_tiling_base> {
    "TilingNode", parent
}
{
    declare_state("running", "tlngStart", "tlngFinish");
    declare_state("suspended", "suspndSend", "rsumedSend");
    connect<&sudoku_tiling_node::_handle_generated<3>>(this, tiles_generated_3);
    connect<&sudoku_tiling_node::_handle_generated<4>>(this, tiles_generated_4);
    connect<&sudoku_tiling_node::_handle_generated<5>>(this, tiles_generated_5);
    connect<&sudoku_tiling_node::_handle_board_timeout>(this, board_timeouted);

    auto& info = provided_endpoint_info();
    info.display_name = "sudoku tiling generator";
    info.description = "sudoku solver tiling generator application";

    setup_connectors(main_context(), *this);
}
//------------------------------------------------------------------------------
void sudoku_tiling_node::active_state(const logger& log) noexcept {
    log.active_state("TilingNode", "running");
}
//------------------------------------------------------------------------------
void sudoku_tiling_node::log_start() noexcept {
    log_change("starting").tag("tlngStart");
}
//------------------------------------------------------------------------------
void sudoku_tiling_node::log_finish() noexcept {
    log_change("finishing").tag("tlngFinish");
}
//------------------------------------------------------------------------------
template <unsigned S>
void sudoku_tiling_node::_handle_generated(
  const eagine::endpoint_id_t,
  const sudoku_tiles<S>& tiles,
  const sudoku_solver_key&) noexcept {
    if(_print_progress) {
        tiles.print_progress(std::cerr) << std::flush;
    }
    if(_print_incomplete or tiles.are_complete()) {
        if(_block_cells) {
            tiles.print(std::cout, block_sudoku_board_traits<S>{}) << std::endl;
        } else {
            tiles.print(std::cout) << std::endl;
        }
    }
    std::string file_path;
    if(
      tiles.are_complete() and
      main_context().config().fetch(
        "msgbus.sudoku.solver.output_path", file_path)) {
        std::ofstream fout{file_path};
        tiles.print(fout) << std::endl;
    }
}
//------------------------------------------------------------------------------
void sudoku_tiling_node::_handle_board_timeout(
  const sudoku_board_timeout& info) noexcept {
    ++_suspend_count;
    this->suspend_send_for(std::chrono::milliseconds{static_cast<std::int32_t>(
      std::log(float(
        1 + info.replaced_board_count + info.pending_board_count +
        _suspend_count * 2)) *
        1000.F +
      1000.F)});
}
//------------------------------------------------------------------------------
} // namespace msgbus
//------------------------------------------------------------------------------
auto main(main_ctx& ctx) -> int {
    signal_switch interrupted;
    msgbus::sudoku_tiling_node::active_state(ctx.log());

    enable_message_bus(ctx);
    ctx.preinitialize();

    msgbus::sudoku_tiling_node tiling_generator(ctx);

    const auto width =
      ctx.config().get<int>("msgbus.sudoku.solver.width").value_or(32);
    const auto height =
      ctx.config().get<int>("msgbus.sudoku.solver.height").value_or(32);

    const auto rank =
      ctx.config().get<int>("msgbus.sudoku.solver.rank").value_or(4);

    auto enqueue{[&](auto traits) {
        tiling_generator.reinitialize(
          {width, height}, traits.make_generator().generate_medium());
    }};

    switch(rank) {
        case 3:
            enqueue(default_sudoku_board_traits<3>());
            break;
        case 4:
            enqueue(default_sudoku_board_traits<4>());
            break;
        case 5:
            enqueue(default_sudoku_board_traits<5>());
            break;
        default:
            ctx.log().error("invalid rank: ${rank}").arg("rank", rank);
            return -1;
    }

    const auto keep_running{[&] {
        return not(interrupted or tiling_generator.tiling_complete());
    }};

    auto alive{ctx.watchdog().start_watch()};

    int idle_streak = 0;

    const auto try_enqueue{[&]<unsigned R>(unsigned_constant<R> r) {
        if(rank == R and tiling_generator.solution_timeouted(r)) [[unlikely]] {
            enqueue(default_sudoku_board_traits<R>());
            tiling_generator.reset_solution_timeout(r);
        }
    }};

    resetting_timeout log_contribution_timeout{
      ctx.config()
        .get<std::chrono::seconds>(
          "msgbus.sudoku.solver.log_contribution_timeout")
        .value_or(std::chrono::minutes{5})};

    tiling_generator.log_start();
    while(keep_running()) {
        tiling_generator.update();
        try_enqueue(unsigned_constant<3>{});
        try_enqueue(unsigned_constant<4>{});
        try_enqueue(unsigned_constant<5>{});

        if(tiling_generator.process_all()) {
            idle_streak = 0;
        } else {
            std::this_thread::sleep_for(
              std::chrono::microseconds(math::minimum(++idle_streak, 50000)));
        }

        if(tiling_generator.bus_node().flow_congestion()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if(log_contribution_timeout) {
            tiling_generator.log_contribution_histogram(rank);
        }

        alive.notify();
    }
    tiling_generator.log_finish();
    tiling_generator.log_contribution_histogram(rank);

    return 0;
}
//------------------------------------------------------------------------------
} // namespace eagine
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    eagine::main_ctx_options options;
    options.app_id = "SudokuTlng";
    return eagine::main_impl(argc, argv, options, &eagine::main);
}
//------------------------------------------------------------------------------

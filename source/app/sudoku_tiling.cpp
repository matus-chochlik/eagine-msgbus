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
    sudoku_tiling_node(main_ctx_parent parent)
      : service_node<sudoku_tiling_base>{"TilingNode", parent} {
        connect<&sudoku_tiling_node::_handle_generated<3>>(
          this, tiles_generated_3);
        connect<&sudoku_tiling_node::_handle_generated<4>>(
          this, tiles_generated_4);
        connect<&sudoku_tiling_node::_handle_generated<5>>(
          this, tiles_generated_5);

        auto& info = provided_endpoint_info();
        info.display_name = "sudoku tiling generator";
        info.description = "sudoku solver tiling generator application";

        setup_connectors(main_context(), *this);
    }

private:
    template <unsigned S>
    void _handle_generated(
      const eagine::identifier_t,
      const sudoku_tiles<S>& tiles,
      const sudoku_solver_key&) noexcept {
        if(_print_progress) {
            tiles.print_progress(std::cerr) << std::flush;
        }
        if(_print_incomplete or tiles.are_complete()) {
            if(_block_cells) {
                tiles.print(std::cout, block_sudoku_board_traits<S>{})
                  << std::endl;
            } else {
                tiles.print(std::cout) << std::endl;
            }
        }
    }

    bool _block_cells{cfg_init("msgbus.sudoku.solver.block_cells", false)};
    bool _print_progress{
      cfg_init("msgbus.sudoku.solver.print_progress", false)};
    bool _print_incomplete{
      cfg_init("msgbus.sudoku.solver.print_incomplete", false)};
};
//------------------------------------------------------------------------------
} // namespace msgbus

auto main(main_ctx& ctx) -> int {
    const signal_switch interrupted;
    enable_message_bus(ctx);
    ctx.preinitialize();

    msgbus::sudoku_tiling_node tiling_generator(ctx);

    const auto width =
      extract_or(ctx.config().get<int>("msgbus.sudoku.solver.width"), 32);
    const auto height =
      extract_or(ctx.config().get<int>("msgbus.sudoku.solver.height"), 32);

    const auto rank =
      extract_or(ctx.config().get<int>("msgbus.sudoku.solver.rank"), 4);

    auto enqueue = [&](auto traits) {
        tiling_generator.reinitialize(
          {width, height}, traits.make_generator().generate_medium());
    };

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

    const auto keep_running = [&] {
        return not(interrupted or tiling_generator.tiling_complete());
    };

    int idle_streak = 0;
    while(keep_running()) {
        tiling_generator.update();
        if(
          rank == 3 and tiling_generator.solution_timeouted(
                          unsigned_constant<3>{})) [[unlikely]] {
            enqueue(default_sudoku_board_traits<3>());
        }
        if(
          rank == 4 and tiling_generator.solution_timeouted(
                          unsigned_constant<4>{})) [[unlikely]] {
            enqueue(default_sudoku_board_traits<4>());
        }
        if(
          rank == 5 and tiling_generator.solution_timeouted(
                          unsigned_constant<5>{})) [[unlikely]] {
            enqueue(default_sudoku_board_traits<5>());
        }
        if(tiling_generator.process_all()) {
            idle_streak = 0;
        } else {
            std::this_thread::sleep_for(
              std::chrono::microseconds(math::minimum(++idle_streak, 50000)));
        }
    }

    switch(rank) {
        case 3:
            tiling_generator.log_contribution_histogram(unsigned_constant<3>{});
            break;
        case 4:
            tiling_generator.log_contribution_histogram(unsigned_constant<4>{});
            break;
        case 5:
            tiling_generator.log_contribution_histogram(unsigned_constant<5>{});
            break;
        default:
            break;
    }

    return 0;
}

} // namespace eagine
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    eagine::main_ctx_options options;
    options.app_id = "SudokuTlng";
    return eagine::main_impl(argc, argv, options, &eagine::main);
}

/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#include <eagine/testing/unit_begin_ctx.hpp>
import eagine.core;
import eagine.msgbus.core;
import eagine.msgbus.services;
//------------------------------------------------------------------------------
template <typename Base = eagine::msgbus::subscriber>
class test_solver : public eagine::msgbus::sudoku_solver<Base> {
    using base = eagine::msgbus::sudoku_solver<Base>;

public:
    test_solver(eagine::msgbus::endpoint& bus)
      : base{bus} {
        connect<&test_solver::check<3>>(this, this->solved_3);
        connect<&test_solver::check<4>>(this, this->solved_4);
    }

    void assign_track(eagitest::track& trck) noexcept {
        _ptrck = &trck;
    }

    template <unsigned S>
    void check(
      const eagine::identifier_t,
      const eagine::msgbus::sudoku_solver_key&,
      eagine::basic_sudoku_board<S>& board) noexcept {
        if(board.is_solved()) {
            if(_ptrck) {
                _ptrck->checkpoint(1);
            }
        }
    }

private:
    eagitest::track* _ptrck{nullptr};
};
//------------------------------------------------------------------------------
// test 1
//------------------------------------------------------------------------------
template <unsigned S>
void sudoku_rank_S_1(auto& s, auto& test) {
    eagitest::track trck{test, 0, 4};
    auto& ctx{s.context()};
    eagine::msgbus::registry the_reg{ctx};

    auto& helper = the_reg.emplace<
      eagine::msgbus::service_composition<eagine::msgbus::sudoku_helper<>>>(
      "Helper");

    if(the_reg.wait_for_id_of(std::chrono::seconds{30}, helper)) {
        auto& solver =
          the_reg.emplace<eagine::msgbus::service_composition<test_solver<>>>(
            "Solver");

        if(the_reg.wait_for_id_of(std::chrono::seconds{30}, solver)) {
            solver.assign_track(trck);

            for(unsigned r = 0; r < test.repeats(3); ++r) {
                solver.enqueue(
                  0,
                  eagine::default_sudoku_board_traits<S>()
                    .make_generator()
                    .generate_medium());

                eagine::timeout solution_timeout{std::chrono::minutes{1}};
                while(not solver.is_done()) {
                    if(solution_timeout.is_expired()) {
                        test.fail("receive timeout");
                        break;
                    }
                    the_reg.update_all();
                    trck.checkpoint(2);
                }
            }
            trck.checkpoint(3);
        } else {
            test.fail("get id solver");
        }

        trck.checkpoint(4);
    } else {
        test.fail("get id helper");
    }

    the_reg.finish();
}
//------------------------------------------------------------------------------
void sudoku_rank_3_1(auto& s) {
    eagitest::case_ test{s, 1, "rank 3"};
    sudoku_rank_S_1<3>(s, test);
}
//------------------------------------------------------------------------------
void sudoku_rank_4_1(auto& s) {
    eagitest::case_ test{s, 2, "rank 4"};
    sudoku_rank_S_1<4>(s, test);
}
//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
auto test_main(eagine::test_ctx& ctx) -> int {
    enable_message_bus(ctx);
    ctx.preinitialize();

    eagitest::ctx_suite test{ctx, "sudoku", 2};
    test.once(sudoku_rank_3_1);
    test.once(sudoku_rank_4_1);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end_ctx.hpp>

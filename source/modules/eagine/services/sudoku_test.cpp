/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
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
      const eagine::msgbus::result_context&,
      const eagine::msgbus::solved_sudoku_board<S>& sol) noexcept {
        if(sol.board.is_solved()) {
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

            solver.enqueue(
              0, eagine::default_sudoku_board_traits<S>().make_diagonal());

            eagine::timeout solution_timeout{std::chrono::minutes{1}};
            while(not solver.is_done()) {
                if(solution_timeout.is_expired()) {
                    test.fail("receive timeout");
                    break;
                }
                the_reg.update_and_process();
                trck.checkpoint(2);
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
// test 2
//------------------------------------------------------------------------------
template <unsigned S>
void sudoku_rank_S_2(auto& s, auto& test, eagine::countdown<> todo) {
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

            eagine::timeout test_timeout{std::chrono::minutes{4}};
            while(todo and not test_timeout.is_expired()) {
                solver.enqueue(
                  0,
                  eagine::default_sudoku_board_traits<S>()
                    .make_generator()
                    .generate_one());

                eagine::timeout solution_timeout{std::chrono::seconds{30}};
                while(not solver.is_done()) {
                    if(solution_timeout.is_expired()) {
                        break;
                    }
                    the_reg.update_and_process();
                    trck.checkpoint(2);
                }
                if(solver.is_done()) {
                    todo.tick();
                }
            }
            if(todo) {
                test.fail("solution timeout");
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
void sudoku_rank_3_2(auto& s) {
    eagitest::case_ test{s, 3, "rank 3"};
    sudoku_rank_S_2<3>(s, test, 3);
}
//------------------------------------------------------------------------------
void sudoku_rank_4_2(auto& s) {
    eagitest::case_ test{s, 4, "rank 4"};
    sudoku_rank_S_2<4>(s, test, 2);
}
//------------------------------------------------------------------------------
// test 3
//------------------------------------------------------------------------------
template <unsigned S>
void sudoku_rank_S_3(auto& s, auto& test, eagine::countdown<> todo) {
    eagitest::track trck{test, 0, 4};
    auto& ctx{s.context()};
    eagine::msgbus::registry the_reg{ctx};

    auto& helper1 = the_reg.emplace<
      eagine::msgbus::service_composition<eagine::msgbus::sudoku_helper<>>>(
      "Helper1");
    auto& helper2 = the_reg.emplace<
      eagine::msgbus::service_composition<eagine::msgbus::sudoku_helper<>>>(
      "Helper2");
    auto& helper3 = the_reg.emplace<
      eagine::msgbus::service_composition<eagine::msgbus::sudoku_helper<>>>(
      "Helper3");

    if(the_reg.wait_for_id_of(
         std::chrono::seconds{30}, helper1, helper2, helper3)) {
        auto& solver =
          the_reg.emplace<eagine::msgbus::service_composition<test_solver<>>>(
            "Solver");

        if(the_reg.wait_for_id_of(std::chrono::seconds{30}, solver)) {
            solver.assign_track(trck);

            eagine::timeout test_timeout{std::chrono::minutes{4}};
            while(todo and not test_timeout.is_expired()) {
                solver.enqueue(
                  0,
                  eagine::default_sudoku_board_traits<S>()
                    .make_generator()
                    .generate_one());

                eagine::timeout solution_timeout{std::chrono::seconds{30}};
                while(not solver.is_done()) {
                    if(solution_timeout.is_expired()) {
                        break;
                    }
                    the_reg.update_and_process();
                    trck.checkpoint(2);
                }
                if(solver.is_done()) {
                    todo.tick();
                }
            }
            if(todo) {
                test.fail("solution timeout");
            }
            trck.checkpoint(3);
        } else {
            test.fail("get id solver");
        }

        trck.checkpoint(4);
    } else {
        test.fail("get id helpers");
    }

    the_reg.finish();
}
//------------------------------------------------------------------------------
void sudoku_rank_3_3(auto& s) {
    eagitest::case_ test{s, 5, "rank 3"};
    sudoku_rank_S_3<3>(s, test, 3);
}
//------------------------------------------------------------------------------
void sudoku_rank_4_3(auto& s) {
    eagitest::case_ test{s, 6, "rank 4"};
    sudoku_rank_S_3<4>(s, test, 1);
}
//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
auto test_main(eagine::test_ctx& ctx) -> int {
    enable_message_bus(ctx);
    ctx.preinitialize();

    eagitest::ctx_suite test{ctx, "sudoku", 6};
    test.once(sudoku_rank_3_1);
    test.once(sudoku_rank_4_1);
    test.once(sudoku_rank_3_2);
    test.once(sudoku_rank_4_2);
    test.once(sudoku_rank_3_3);
    test.once(sudoku_rank_4_3);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end_ctx.hpp>

/// @example eagine/msgbus/012_sudoku_threads.cpp
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
import eagine.core;
import eagine.sslplus;
import eagine.msgbus;
import <atomic>;
import <iostream>;
import <mutex>;
import <thread>;

namespace eagine {
namespace msgbus {

using example_helper = service_composition<sudoku_helper<>>;

class example_solver : public service_composition<sudoku_solver<>> {
public:
    using base = service_composition<sudoku_solver<>>;

    example_solver(endpoint& bus)
      : base{bus} {
        connect<&example_solver::print<3>>(this, solved_3);
        connect<&example_solver::print<4>>(this, solved_4);
        connect<&example_solver::print<5>>(this, solved_5);
    }

    template <unsigned S>
    void print(
      const identifier_t,
      const sudoku_solver_key& id,
      basic_sudoku_board<S>& board) noexcept {
        std::cout << "board: " << std::get<int>(id) << '\n'
                  << board << std::endl;
    }
};

} // namespace msgbus

auto main(main_ctx& ctx) -> int {
    const auto worker_count =
      extract_or(ctx.system().cpu_concurrent_threads(), 4) + 1;

    auto acceptor = msgbus::make_direct_acceptor(ctx);

    msgbus::endpoint solver_endpoint("Solver", ctx);
    solver_endpoint.add_connection(acceptor->make_connection());
    msgbus::example_solver solver(solver_endpoint);

    auto board_count = 5;
    ctx.args().find("--count").parse_next(board_count, std::cerr);

    const auto enqueue = [&](auto generator) {
        for(int id = 0; id < board_count; ++id) {
            solver.enqueue(id, generator.generate_medium());
        }
    };

    if(ctx.args().find("--3")) {
        enqueue(default_sudoku_board_traits<3>().make_generator());
    }

    if(ctx.args().find("--4")) {
        enqueue(default_sudoku_board_traits<4>().make_generator());
    }

    if(ctx.args().find("--5")) {
        enqueue(default_sudoku_board_traits<5>().make_generator());
    }

    std::mutex worker_mutex;
    std::atomic<bool> start = false;
    std::atomic<bool> done = false;
    std::vector<std::thread> workers;
    workers.reserve(worker_count);

    for(span_size_t i = 0; i < worker_count; ++i) {
        workers.emplace_back(
          [&worker_mutex,
           &start,
           &done,
           helper_obj{main_ctx_object{"Helper", ctx}},
           connection{acceptor->make_connection()}]() mutable {
              worker_mutex.lock();
              msgbus::endpoint helper_endpoint{std::move(helper_obj)};
              helper_endpoint.add_connection(std::move(connection));
              msgbus::example_helper helper(helper_endpoint);
              helper.update();
              worker_mutex.unlock();

              while(not start) {
                  std::this_thread::sleep_for(std::chrono::milliseconds(100));
              }

              while(not done) {
                  helper.update();
                  if(not helper.process_all()) {
                      std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  }
              }
          });
    }

    worker_mutex.lock();
    msgbus::router router(ctx);
    router.add_acceptor(std::move(acceptor));
    router.update();
    worker_mutex.unlock();

    start = true;
    while(not solver.is_done()) {
        router.update();
        solver.update();
        solver.process_all();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    done = true;
    for(auto& worker : workers) {
        worker.join();
    }

    return 0;
}
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    return eagine::default_main(argc, argv, eagine::main);
}


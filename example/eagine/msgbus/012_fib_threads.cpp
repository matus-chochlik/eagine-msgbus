/// @example eagine/msgbus/012_fib_threads.cpp
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
import eagine.core;
import eagine.sslplus;
import eagine.msgbus;
import <queue>;
import <set>;
import <thread>;

namespace eagine {
namespace msgbus {
//------------------------------------------------------------------------------
class fibonacci_server : public actor<3> {
public:
    using base = actor<3>;
    using base::bus_node;

    fibonacci_server(main_ctx_object obj)
      : base(
          std::move(obj),
          this,
          message_map<"Fibonacci", "FindServer", &fibonacci_server::is_ready>{},
          message_map<"Fibonacci", "Calculate", &fibonacci_server::calculate>{},
          message_map<"Fibonacci", "Shutdown", &fibonacci_server::shutdown>{}) {
    }

    auto shutdown(const message_context&, const stored_message&) noexcept
      -> bool {
        _done = true;
        return true;
    }

    auto is_ready(const message_context&, const stored_message& msg_in) noexcept
      -> bool {
        bus_node().respond_to(msg_in, {"Fibonacci", "IsReady"});
        return true;
    }

    static auto fib(const std::int64_t arg) noexcept -> std::int64_t {
        return arg <= 2 ? 1 : fib(arg - 2) + fib(arg - 1);
    }

    auto calculate(const message_context&, const stored_message& msg_in) noexcept
      -> bool {
        std::int64_t arg{0};
        std::int64_t result{0};
        // deserialize
        block_data_source source(msg_in.content());
        fast_deserializer_backend read_backend(source);
        deserialize(arg, read_backend);
        // call
        result = fib(arg);
        // serialize
        std::array<byte, 64> buffer{};
        block_data_sink sink(cover(buffer));
        fast_serializer_backend write_backend(sink);
        serialize(std::tie(arg, result), write_backend);
        // send
        message_view msg_out{sink.done()};
        msg_out.set_serializer_id(write_backend.type_id());
        bus_node().respond_to(msg_in, {"Fibonacci", "Result"}, msg_out);
        return true;
    }

    auto is_done() const noexcept {
        return _done;
    }

private:
    bool _done{false};
};
//------------------------------------------------------------------------------
class fibonacci_client : public actor<2> {
public:
    using base = actor<2>;
    using base::bus_node;

    fibonacci_client(main_ctx_object obj)
      : base(
          std::move(obj),
          this,
          message_map<"Fibonacci", "IsReady", &fibonacci_client::dispatch>{},
          message_map<"Fibonacci", "Result", &fibonacci_client::print>{}) {}

    void enqueue(const std::int64_t arg) {
        _remaining.push(arg);
    }

    void shutdown() {
        bus_node().broadcast({"Fibonacci", "Shutdown"});
    }

    void update() {
        if(!_remaining.empty()) {
            bus_node().broadcast({"Fibonacci", "FindServer"});
        }
    }

    auto dispatch(const message_context&, const stored_message& msg_in) noexcept
      -> bool {
        if(!_remaining.empty()) {
            auto arg = _remaining.front();
            _pending.insert(arg);
            _remaining.pop();
            // serialize
            std::array<byte, 32> buffer{};
            block_data_sink sink(cover(buffer));
            fast_serializer_backend write_backend(sink);
            serialize(arg, write_backend);
            message_view msg_out{sink.done()};
            msg_out.set_serializer_id(write_backend.type_id());
            bus_node().respond_to(msg_in, {"Fibonacci", "Calculate"}, msg_out);
        }
        return true;
    }

    auto print(const message_context&, const stored_message& msg_in) noexcept
      -> bool {
        std::int64_t arg{0};
        std::int64_t result{0};
        // deserialize
        block_data_source source(msg_in.content());
        fast_deserializer_backend read_backend(source);
        auto tup = std::tie(arg, result);
        deserialize(tup, read_backend);
        // print
        bus_node()
          .cio_print("fib(${arg}) = ${fib}")
          .arg("arg", arg)
          .arg("fib", result);
        // remove
        _pending.erase(arg);
        return true;
    }

    auto is_done() const -> bool {
        return _remaining.empty() && _pending.empty();
    }

private:
    std::queue<std::int64_t> _remaining{};
    std::set<std::int64_t> _pending{};
};
//------------------------------------------------------------------------------
} // namespace msgbus

auto main(main_ctx& ctx) -> int {
    enable_message_bus(ctx);

    const auto thread_count =
      extract_or(ctx.system().cpu_concurrent_threads(), 4);

    auto acceptor = std::make_unique<msgbus::direct_acceptor>(ctx);

    msgbus::fibonacci_client client({"FibClient", ctx});
    client.add_connection(acceptor->make_connection());

    std::vector<std::thread> workers;
    workers.reserve(thread_count);

    for(span_size_t i = 0; i < thread_count; ++i) {
        workers.emplace_back([srv_obj{main_ctx_object{"FibServer", ctx}},
                              connection{
                                acceptor->make_connection()}]() mutable {
            msgbus::fibonacci_server server(std::move(srv_obj));
            server.add_connection(std::move(connection));

            while(!server.is_done()) {
                if(!server.process_one()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        });
    }

    msgbus::router router(ctx);
    router.add_acceptor(std::move(acceptor));

    const std::int64_t n = running_on_valgrind() ? 34 : 46;

    for(std::int64_t i = 1; i <= n; ++i) {
        client.enqueue(i);
    }

    while(!client.is_done()) {
        router.update();
        client.update();
        if(!client.process_one()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    client.shutdown();
    router.update();

    for(auto& worker : workers) {
        worker.join();
    }

    return 0;
}
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    return eagine::default_main(argc, argv, eagine::main);
}


/// @example eagine/msgbus/009_fib_service.cpp
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

namespace eagine {
namespace msgbus {
//------------------------------------------------------------------------------
struct fibonacci_server : static_subscriber<2> {
    using base = static_subscriber<2>;
    using base::bus_node;

    fibonacci_server(endpoint& ep)
      : base(
          ep,
          this,
          message_map<"Fibonacci", "FindServer", &fibonacci_server::is_ready>{},
          message_map<"Fibonacci", "Calculate", &fibonacci_server::calculate>{}) {
    }

    auto is_ready(const message_context&, const stored_message& msg_in) noexcept
      -> bool {
        bus_node().respond_to(msg_in, {"Fibonacci", "IsReady"});
        return true;
    }

    static auto fib(const std::int64_t arg) -> std::int64_t {
        return arg <= 2 ? 1 : fib(arg - 2) + fib(arg - 1);
    }

    auto calculate(const message_context&, const stored_message& msg_in) noexcept
      -> bool {
        skeleton<
          std::int64_t(std::int64_t),
          fast_serializer_backend,
          fast_deserializer_backend,
          block_data_sink,
          block_data_source,
          64>()
          .call(bus_node(), msg_in, {"Fibonacci", "Result"}, {&fib});
        return true;
    }
};
//------------------------------------------------------------------------------
struct fibonacci_client : static_subscriber<2> {
    using base = static_subscriber<2>;
    using base::bus_node;

    fibonacci_client(endpoint& ep)
      : base(
          ep,
          this,
          message_map<"Fibonacci", "IsReady", &fibonacci_client::dispatch>{},
          message_map<"Fibonacci", "Result", &fibonacci_client::fulfill>{}) {}

    void enqueue(const std::int64_t arg) {
        _remaining.push(arg);
    }

    void update() {
        if(!_remaining.empty()) {
            bus_node().broadcast({"Fibonacci", "FindServer"});
        }
    }

    auto dispatch(const message_context&, const stored_message& msg_in) noexcept
      -> bool {
        if(!_remaining.empty()) {
            const auto arg = _remaining.front();
            _remaining.pop();

            _calc_invoker
              .invoke_on(
                bus_node(), msg_in.source_id, {"Fibonacci", "Calculate"}, arg)
              .set_timeout(std::chrono::minutes(1))
              .on_timeout([this, arg]() { this->_remaining.push(arg); })
              .then([this, arg](std::int64_t fib) {
                  bus_node()
                    .cio_print("fib(${arg}) = ${fib}")
                    .arg("arg", arg)
                    .arg("fib", fib);
              });
        }
        return true;
    }

    auto fulfill(
      const message_context& ctx,
      const stored_message& message) noexcept -> bool {
        _calc_invoker.fulfill_by(ctx, message);
        return true;
    }

    auto is_done() const {
        return _remaining.empty() && _calc_invoker.is_done();
    }

private:
    invoker<
      std::int64_t(std::int64_t),
      fast_serializer_backend,
      fast_deserializer_backend,
      block_data_sink,
      block_data_source,
      64>
      _calc_invoker{};
    std::queue<std::int64_t> _remaining{};
};
//------------------------------------------------------------------------------
} // namespace msgbus

auto main(main_ctx& ctx) -> int {

    auto acceptor = msgbus::make_direct_acceptor(ctx);

    msgbus::endpoint server_endpoint("Server", ctx);
    msgbus::endpoint client_endpoint("Client", ctx);

    server_endpoint.add_connection(acceptor->make_connection());
    client_endpoint.add_connection(acceptor->make_connection());

    msgbus::router router(ctx);
    router.add_acceptor(std::move(acceptor));

    msgbus::fibonacci_server server(server_endpoint);
    msgbus::fibonacci_client client(client_endpoint);

    const std::int64_t n = running_on_valgrind() ? 36 : 45;

    for(std::int64_t i = 1; i <= n; ++i) {
        client.enqueue(i);
    }

    while(!client.is_done()) {
        router.update();
        client_endpoint.update();
        server_endpoint.update();
        client.update();
        client.process_one();
        server.process_one();
    }

    return 0;
}
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    return eagine::default_main(argc, argv, eagine::main);
}


/// @example eagine/msgbus/010_fib_async.cpp
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
import eagine.core;
import eagine.sslplus;
import eagine.msgbus;
import std;

namespace eagine {
namespace msgbus {
//------------------------------------------------------------------------------
template <typename Base = subscriber>
class fibonacci_server_impl : public Base {

protected:
    using Base::Base;
    using Base::bus_node;

    void add_methods() {
        Base::add_methods();
        Base::add_method(
          this,
          message_map<
            "Fibonacci",
            "Calculate",
            &fibonacci_server_impl::calculate>{});
        _workers.populate();
    }

private:
    workshop _workers{};
    default_async_skeleton<std::int64_t(std::int64_t), 64> _calc_skeleton{};

    static auto fib(std::int64_t arg) -> std::int64_t {
        return arg <= 2 ? 1 : fib(arg - 2) + fib(arg - 1);
    }

    auto calculate(
      const message_context&,
      const stored_message& msg_in) noexcept {
        _calc_skeleton.enqueue(
          msg_in, {"Fibonacci", "Result"}, {&fib}, _workers);
        return true;
    }

public:
    auto update() {
        some_true something_done{};
        something_done(Base::update());

        something_done(_calc_skeleton.handle_one(bus_node()));

        return something_done;
    }
};
using fibonacci_server = service_composition<fibonacci_server_impl<>>;
//------------------------------------------------------------------------------
template <typename Base = subscriber>
class fibonacci_client_impl : public Base {
    using This = fibonacci_client_impl;

protected:
    using Base::Base;
    using Base::bus_node;

    void add_methods() {
        Base::add_methods();
        Base::add_method(_calc_invoker.map_fulfill_by({"Fibonacci", "Result"}));
    }

private:
    default_invoker<std::int64_t(std::int64_t), 64> _calc_invoker{};

public:
    auto fib(const std::int64_t arg) -> future<std::int64_t> {
        return _calc_invoker.invoke(
          bus_node(), {"Fibonacci", "Calculate"}, arg);
    }

    auto is_done() const -> bool {
        return _calc_invoker.is_done();
    }
};
using fibonacci_client = service_composition<fibonacci_client_impl<>>;
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

    const std::int64_t n = running_on_valgrind() ? 40 : 50;

    for(std::int64_t i = 1; i <= n; ++i) {
        client.fib(i)
          .set_timeout(std::chrono::minutes(1))
          .then([i, &ctx](std::int64_t fib) {
              ctx.cio()
                .print("MsgBus", "fib(${arg}) = ${fib}")
                .arg("arg", i)
                .arg("fib", fib);
          })
          .otherwise([&ctx]() { ctx.cio().print("MsgBus", "failed"); });
    }

    while(not client.is_done()) {
        router.update();
        client.update();
        server.update();
        client.process_one();
        server.process_one();
    }

    return 0;
}
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    return eagine::default_main(argc, argv, eagine::main);
}


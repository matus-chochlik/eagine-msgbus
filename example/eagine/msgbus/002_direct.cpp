/// @example eagine/msgbus/002_direct.cpp
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
#if EAGINE_MSGBUS_MODULE
import eagine.core;
import eagine.msgbus;
#else
#include <eagine/main_ctx.hpp>
#include <eagine/memory/span_algo.hpp>
#include <eagine/msgbus/acceptor.hpp>
#include <eagine/msgbus/direct.hpp>
#include <eagine/msgbus/endpoint.hpp>
#include <eagine/msgbus/router.hpp>
#include <eagine/msgbus/subscriber.hpp>
#endif

namespace eagine {
namespace msgbus {
//------------------------------------------------------------------------------
struct str_utils_server
  : main_ctx_object
  , static_subscriber<2> {
    using base = static_subscriber<2>;
    using base::bus_node;

    str_utils_server(endpoint& ep)
      : main_ctx_object{identifier{"Server"}, ep}
      , base{
          ep,
          this,
          message_map<
            id_v("StrUtilReq"),
            id_v("UpperCase"),
            &str_utils_server::uppercase>{},
          message_map<
            id_v("StrUtilReq"),
            id_v("Reverse"),
            &str_utils_server::reverse>{}} {}

    auto reverse(const message_context&, const stored_message& msg) noexcept
      -> bool {
        auto str = as_chars(copy(msg.content(), _buf));
        log_trace("received request: ${content}")
          .arg(identifier{"content"}, str);
        memory::reverse(str);
        bus_node().post(message_id{"StrUtilRes", "Reverse"}, as_bytes(str));
        return true;
    }

    auto uppercase(const message_context&, const stored_message& msg) noexcept
      -> bool {
        auto str = as_chars(copy(msg.content(), _buf));
        transform(str, [](char x) { return char(std::toupper(x)); });
        bus_node().post(message_id{"StrUtilRes", "UpperCase"}, as_bytes(str));
        return true;
    }

private:
    memory::buffer _buf;
};
//------------------------------------------------------------------------------
struct str_utils_client
  : main_ctx_object
  , static_subscriber<2> {
    using this_class = str_utils_client;
    using base = static_subscriber<2>;
    using base::bus_node;

    str_utils_client(endpoint& ep)
      : main_ctx_object{identifier{"Client"}, ep}
      , base(
          ep,
          this,
          message_map<
            id_v("StrUtilRes"),
            id_v("UpperCase"),
            &str_utils_client::print>{},
          message_map<
            id_v("StrUtilRes"),
            id_v("Reverse"),
            &str_utils_client::print>{}) {}

    void call_reverse(const string_view str) {
        ++_remaining;
        bus_node().post(message_id{"StrUtilReq", "Reverse"}, as_bytes(str));
    }

    void call_uppercase(const string_view str) {
        ++_remaining;
        bus_node().post(message_id{"StrUtilReq", "UpperCase"}, as_bytes(str));
    }

    auto print(const message_context&, const stored_message& msg) noexcept
      -> bool {
        log_info("received response: ${content}")
          .arg(identifier{"content"}, msg.text_content());
        --_remaining;
        return true;
    }

    auto is_done() const -> bool {
        return _remaining <= 0;
    }

private:
    logger _log{};
    int _remaining{0};
};
//------------------------------------------------------------------------------
} // namespace msgbus

auto main(main_ctx& ctx) -> int {
    auto acceptor = std::make_unique<msgbus::direct_acceptor>(ctx);

    msgbus::endpoint server_endpoint{identifier{"ServerEp"}, ctx};
    msgbus::endpoint client_endpoint{identifier{"ClientEp"}, ctx};

    server_endpoint.add_connection(acceptor->make_connection());
    client_endpoint.add_connection(acceptor->make_connection());

    msgbus::router router(ctx);
    router.add_acceptor(std::move(acceptor));

    msgbus::str_utils_server server(server_endpoint);
    msgbus::str_utils_client client(client_endpoint);

    client.call_reverse("foo");
    client.call_reverse("bar");
    client.call_reverse("baz");
    client.call_reverse("qux");

    client.call_uppercase("foo");
    client.call_uppercase("bar");
    client.call_uppercase("baz");
    client.call_uppercase("qux");

    while(!client.is_done()) {
        router.update();
        server_endpoint.update();
        client_endpoint.update();
        server.process_one();
        client.process_one();
    }

    return 0;
}
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    return eagine::default_main(argc, argv, eagine::main);
}


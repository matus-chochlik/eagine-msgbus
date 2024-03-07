/// @example eagine/msgbus/001_loopback.cpp
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
import eagine.core;
import eagine.msgbus;
import std;

namespace eagine {
namespace msgbus {
//------------------------------------------------------------------------------
struct str_utils_server
  : main_ctx_object
  , static_subscriber<1> {
    using this_class = str_utils_server;
    using base = static_subscriber<1>;
    using base::bus_node;

    str_utils_server(endpoint& ep)
      : main_ctx_object{identifier{"Server"}, ep}
      , base(
          ep,
          this,
          message_map<"StrUtilReq", "Reverse", &this_class::reverse>{}) {}

    auto reverse(const message_context&, const stored_message& msg) noexcept
      -> bool {
        auto str = as_chars(copy(msg.content(), _buf));
        log_trace("received request: ${content}")
          .arg(identifier{"content"}, str);
        memory::reverse(str);
        bus_node().post(message_id{"StrUtilRes", "Reverse"}, as_bytes(str));
        return true;
    }

private:
    memory::buffer _buf;
};
//------------------------------------------------------------------------------
struct str_utils_client
  : main_ctx_object
  , static_subscriber<1> {
    using this_class = str_utils_client;
    using base = static_subscriber<1>;
    using base::bus_node;

    str_utils_client(endpoint& ep)
      : main_ctx_object{identifier{"Client"}, ep}
      , base{
          ep,
          this,
          message_map<"StrUtilRes", "Reverse", &this_class::print>{}} {}

    void call_reverse(const string_view str) {
        ++_remaining;
        bus_node().post(message_id{"StrUtilReq", "Reverse"}, as_bytes(str));
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
    int _remaining{0};
};
//------------------------------------------------------------------------------
} // namespace msgbus

auto main(main_ctx& ctx) -> int {

    msgbus::endpoint bus{identifier{"Loopback"}, ctx};
    bus.set_id(identifier{"BusExample"});
    bus.add_connection(hold<msgbus::loopback_connection>);

    msgbus::str_utils_server server(bus);
    msgbus::str_utils_client client(bus);

    client.call_reverse("foo");
    client.call_reverse("bar");
    client.call_reverse("baz");
    client.call_reverse("qux");

    while(not client.is_done()) {
        bus.update();
        server.process_one();
        client.process_one();
    }

    return 0;
}

} // namespace eagine

auto main(int argc, const char** argv) -> int {
    return eagine::default_main(argc, argv, eagine::main);
}


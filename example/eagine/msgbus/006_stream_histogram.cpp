/// @example eagine/msgbus/006_stream_histogram.cpp
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
import eagine.core;
import eagine.sslplus;
import eagine.msgbus;
import <chrono>;
import <thread>;

namespace eagine {

auto main(main_ctx& ctx) -> int {
    enable_message_bus(ctx);

    timeout idle_too_long{std::chrono::seconds{30}};

    msgbus::resource_data_consumer_node node{ctx};
    msgbus::setup_connectors(ctx, node);

    auto enqueue = [&](url locator) {
        if(locator) {
            node.stream_resource(
              std::move(locator),
              msgbus::message_priority::high,
              std::chrono::hours{1});
        }
    };

    for(auto& arg : ctx.args()) {
        enqueue(url(arg.get_string()));
    }
    if(!node.has_pending_resources()) {
        enqueue(url("eagires:///zeroes?count=1073741824"));
    }

    const auto is_done = [&] {
        if(idle_too_long || !node.has_pending_resources()) {
            return true;
        }
        return false;
    };

    while(!is_done()) {
        if(node.update_and_process_all()) {
            idle_too_long.reset();
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    return 0;
}

} // namespace eagine

auto main(int argc, const char** argv) -> int {
    eagine::main_ctx_options options;
    options.app_id = "RsrcExmple";
    return eagine::main_impl(argc, argv, options, eagine::main);
}

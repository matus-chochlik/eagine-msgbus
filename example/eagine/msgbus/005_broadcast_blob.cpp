/// @example eagine/msgbus/005_broadcast_blob.cpp
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

    msgbus::endpoint bus{"Temporary", ctx};
    msgbus::setup_connectors(ctx, bus);

    if(ctx.args().none()) {
        file_contents data(ctx.exe_path());

        bus.broadcast_blob(
          message_id{"Example", "Content"}, data, std::chrono::minutes(5));
    } else {
        for(auto& arg : ctx.args()) {
            if(file_contents data{arg}) {

                bus.broadcast_blob(
                  message_id{"Example", "Content"},
                  data,
                  std::chrono::minutes(5));
            }
        }
    }

    timeout done{std::chrono::seconds{3}};
    while(not done) {
        if(bus.update()) {
            done.reset();
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    bus.finish();

    return 0;
}

} // namespace eagine

auto main(int argc, const char** argv) -> int {
    return eagine::default_main(argc, argv, eagine::main);
}


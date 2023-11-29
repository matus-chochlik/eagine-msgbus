///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

import eagine.core;
import eagine.sslplus;
import eagine.msgbus;
import std;

namespace eagine {
//------------------------------------------------------------------------------
auto main(main_ctx& ctx) -> int {
    signal_switch interrupted;
    const auto sig_bind{ctx.log().log_when_switched(interrupted)};

    enable_message_bus(ctx);
    ctx.preinitialize();

    msgbus::router_address address{ctx};
    msgbus::connection_setup conn_setup(ctx);

    msgbus::endpoint bus{main_ctx_object{"FilSvrEndp", ctx}};

    msgbus::resource_data_server_node the_file_server{bus};
    conn_setup.setup_connectors(the_file_server, address);

    auto alive{ctx.watchdog().start_watch()};

    while(not(the_file_server.is_done() or interrupted)) {
        if(the_file_server.update_message_age().update_and_process_all()) {
            std::this_thread::sleep_for(std::chrono::microseconds(125));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        alive.notify();
    }

    return 0;
}
//------------------------------------------------------------------------------
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    eagine::main_ctx_options options;
    options.app_id = "FileServer";
    return eagine::main_impl(argc, argv, options, &eagine::main);
}

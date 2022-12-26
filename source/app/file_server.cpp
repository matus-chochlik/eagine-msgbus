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
import <filesystem>;
import <memory>;
import <thread>;

namespace eagine {
//------------------------------------------------------------------------------
auto main(main_ctx& ctx) -> int {
    const signal_switch interrupted;
    enable_message_bus(ctx);
    ctx.preinitialize();

    msgbus::router_address address{ctx};
    msgbus::connection_setup conn_setup(ctx);

    msgbus::endpoint bus{main_ctx_object{"FilSvrEndp", ctx}};

    msgbus::resource_data_server_node the_file_server{bus};
    conn_setup.setup_connectors(the_file_server, address);

    auto& wd = ctx.watchdog();
    wd.declare_initialized();

    while(not(the_file_server.is_done() or interrupted)) {
        the_file_server.average_message_age(
          the_file_server.bus_node().flow_average_message_age());
        if(the_file_server.update_and_process_all()) {
            std::this_thread::sleep_for(std::chrono::microseconds(125));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        wd.notify_alive();
    }
    wd.announce_shutdown();

    return 0;
}
//------------------------------------------------------------------------------
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    eagine::main_ctx_options options;
    options.app_id = "FileServer";
    return eagine::main_impl(argc, argv, options, &eagine::main);
}

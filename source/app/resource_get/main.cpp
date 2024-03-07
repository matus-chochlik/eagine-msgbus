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

auto main(main_ctx& ctx) -> int {
    if(const auto exit_code{handle_common_special_args(ctx)}) {
        return *exit_code;
    }

    signal_switch interrupted;
    const auto sig_bind{ctx.log().log_when_switched(interrupted)};

    enable_message_bus(ctx);

    timeout idle_too_long{std::chrono::seconds{30}};

    msgbus::endpoint bus{"RsrcClient", ctx};
    msgbus::resource_data_consumer_node node{bus};
    msgbus::setup_connectors(ctx, node);
    const application_config_value<std::chrono::seconds> blob_timeout{
      ctx.config(), "msgbus.resource_get.blob_timeout", std::chrono::hours{12}};

    auto next_arg{[arg{ctx.args().first()}] mutable {
        while(arg) {
            arg = arg.next();
            if(arg.prev().is_long_tag("url")) {
                break;
            }
        }
        return arg;
    }};

    const auto enqueue{[&](url locator) {
        if(locator) {
            node.stream_resource(
              std::move(locator),
              msgbus::message_priority::critical,
              blob_timeout);
        }
    }};

    const auto enqueue_next{[&] {
        if(const auto arg{next_arg()}) {
            enqueue(url{arg});
        }
    }};

    const auto consume{[&](const msgbus::blob_stream_chunk& chunk) {
        for(const auto blk : chunk.data) {
            write_to_stream(std::cout, blk);
        }
    }};
    node.blob_stream_data_appended.connect({construct_from, consume});

    const auto blob_done{[&](identifier_t) {
        std::cout << std::endl;
        enqueue_next();
    }};
    node.blob_stream_finished.connect({construct_from, blob_done});
    node.blob_stream_cancelled.connect({construct_from, blob_done});

    const auto is_done{[&] {
        return interrupted or idle_too_long or not node.has_pending_resources();
    }};

    enqueue_next();

    while(not is_done()) {
        if(node.update_and_process_all()) {
            idle_too_long.reset();
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
    }

    return 0;
}

} // namespace eagine

auto main(int argc, const char** argv) -> int {
    eagine::main_ctx_options options;
    options.app_id = "RsourceGet";
    return eagine::main_impl(argc, argv, options, eagine::main);
}

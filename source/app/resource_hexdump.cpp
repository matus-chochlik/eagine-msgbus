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

auto main(main_ctx& ctx) -> int {
    enable_message_bus(ctx);

    timeout idle_too_long{std::chrono::seconds{30}};

    msgbus::endpoint bus{"StrmClient", ctx};
    msgbus::resource_data_consumer_node node{bus};
    msgbus::setup_connectors(ctx, node);

    auto next_arg{[arg{ctx.args().first()}] mutable {
        auto result{arg};
        arg = arg.next();
        return result;
    }};

    const auto enqueue{[&](url locator) {
        if(locator) {
            node.fetch_resource_chunks(
              std::move(locator),
              ctx.default_chunk_size(),
              msgbus::message_priority::normal,
              std::chrono::hours{1});
        }
    }};

    const auto enqueue_next{[&] {
        if(const auto arg{next_arg()}) {
            enqueue(url{arg.get_string()});
        }
    }};

    const auto consume{[&](const msgbus::blob_stream_chunk& chunk) {
        for(const auto blk : chunk.data) {
            std::cout << hexdump(blk) << std::endl;
        }
    }};
    node.blob_stream_data_appended.connect({construct_from, consume});

    const auto blob_done{[&](identifier_t) {
        enqueue_next();
    }};
    node.blob_stream_finished.connect({construct_from, blob_done});
    node.blob_stream_cancelled.connect({construct_from, blob_done});

    const auto is_done{[&] {
        return idle_too_long or not node.has_pending_resources();
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
    options.app_id = "RsrcHexDmp";
    return eagine::main_impl(argc, argv, options, eagine::main);
}

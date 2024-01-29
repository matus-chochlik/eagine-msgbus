/// @example eagine/msgbus/005_byte_histogram.cpp
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
    enable_message_bus(ctx);

    std::array<span_size_t, 256> byte_counts{};

    auto log_byte_hist = [&ctx, &byte_counts](
                           const msgbus::message_context& mc,
                           const msgbus::stored_message& msg) {
        if(msg.data().size()) {
            zero(cover(byte_counts));

            span_size_t max_count{0};
            for(auto b : msg.content()) {
                max_count =
                  math::maximum(max_count, ++byte_counts[std_size(b)]);
            }

            ctx.log()
              .info("received blob message ${message}")
              .arg("message", mc.msg_id())
              .arg_func([&byte_counts, max_count](logger_backend& backend) {
                  for(const auto i : integer_range(std_size(256))) {
                      backend.add_float(
                        byte_to_identifier(byte(i)),
                        "Histogram",
                        float(0),
                        float(byte_counts[i]),
                        float(max_count));
                  }
              });
        }

        return true;
    };

    msgbus::endpoint bus{main_ctx_object{"Temporary", ctx}};

    msgbus::setup_connectors(ctx, bus);

    timeout idle_too_long{std::chrono::seconds{30}};
    while(not idle_too_long) {
        if(
          bus.update() or
          bus.process_everything({construct_from, log_byte_hist})) {
            idle_too_long.reset();
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


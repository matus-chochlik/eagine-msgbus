/// @example eagine/msgbus/008_file_histogram.cpp
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

class example_blob_io : public target_blob_io {
public:
    example_blob_io(const logger& log, url loc) noexcept
      : _log{log}
      , _locator{std::move(loc)} {
        zero(cover(_byte_counts));
    }

    auto store_fragment(
      const span_size_t,
      const memory::const_block src,
      const blob_info&) noexcept -> bool final {
        for(auto b : src) {
            _max_count = math::maximum(_max_count, ++_byte_counts[std_size(b)]);
        }
        return true;
    }

    void handle_finished(
      const message_id,
      const message_age,
      const message_info&,
      const blob_info&) noexcept final {
        _finished = true;
        _log.info("blob byte counts")
          .arg("url", "URL", _locator.str())
          .arg_func([this](logger_backend& backend) {
              for(const auto i : integer_range(std_size(256))) {
                  if(_byte_counts[i]) {
                      backend.add_float(
                        byte_to_identifier(byte(i)),
                        "Histogram",
                        float(0),
                        float(_byte_counts[i]),
                        float(_max_count));
                  }
              }
          });
    }

    void handle_cancelled() noexcept final {
        _finished = true;
    }

    void activate() noexcept {
        _active = true;
    }

    auto is_active() const noexcept -> bool {
        return _active;
    }

    auto is_done() const noexcept -> bool {
        return _finished;
    }

    auto locator() const noexcept -> const url& {
        return _locator;
    }

private:
    const logger& _log;
    url _locator;
    span_size_t _max_count{0};
    std::array<span_size_t, 256> _byte_counts{};
    bool _active{false};
    bool _finished{false};
};

using manipulator_node = service_node<resource_manipulator<>>;

} // namespace msgbus

auto main(main_ctx& ctx) -> int {
    enable_message_bus(ctx);

    timeout idle_too_long{std::chrono::seconds{30}};
    std::vector<std::shared_ptr<msgbus::example_blob_io>> blobs;

    for(auto& arg : ctx.args()) {
        if(url locator{arg}) {
            blobs.emplace_back(std::make_unique<msgbus::example_blob_io>(
              ctx.log(), std::move(locator)));
        }
    }
    if(blobs.empty()) {
        blobs.emplace_back(std::make_unique<msgbus::example_blob_io>(
          ctx.log(), url("eagires:///zeroes?count=1073741824")));
    }

    const auto is_done = [&] {
        if(idle_too_long) {
            return true;
        }
        for(const auto& blob : blobs) {
            if(not blob->is_done()) {
                return false;
            }
        }
        return true;
    };

    msgbus::manipulator_node node{"FileManip", ctx};
    msgbus::setup_connectors(ctx, node);

    const auto on_server_appeared{[&](endpoint_id_t endpoint_id) {
        for(const auto& blob_io : blobs) {
            if(not blob_io->is_done()) {
                node.search_resource(endpoint_id, blob_io->locator());
            }
        }
    }};
    node.resource_server_appeared.connect({construct_from, on_server_appeared});

    const auto on_resource_found{
      [&](endpoint_id_t endpoint_id, const url& locator) {
          for(const auto& blob_io : blobs) {
              if(not blob_io->is_active() and not blob_io->is_done()) {
                  if(blob_io->locator() == locator) {
                      blob_io->activate();
                      node.query_resource_content(
                        endpoint_id,
                        blob_io->locator(),
                        blob_io,
                        msgbus::message_priority::high,
                        std::chrono::hours{1});
                  }
              }
          }
      }};
    node.server_has_resource.connect({construct_from, on_resource_found});

    const auto on_resource_missing = [&](endpoint_id_t, const url& locator) {
        for(const auto& blob_io : blobs) {
            if(not blob_io->is_active() and not blob_io->is_done()) {
                if(blob_io->locator() == locator) {
                    blob_io->handle_cancelled();
                }
            }
        }
    };
    node.server_has_not_resource.connect({construct_from, on_resource_missing});

    while(not is_done()) {
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
    options.app_id = "FileManExe";
    return eagine::main_impl(argc, argv, options, eagine::main);
}

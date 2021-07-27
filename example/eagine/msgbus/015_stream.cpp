/// @example eagine/msgbus/015_stream.cpp
/// @note This example and the streaming system is work in progress
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
#include <eagine/flat_set.hpp>
#include <eagine/main_ctx.hpp>
#include <eagine/main_fwd.hpp>
#include <eagine/message_bus.hpp>
#include <eagine/msgbus/registry.hpp>
#include <eagine/msgbus/service.hpp>
#include <eagine/msgbus/service/stream.hpp>
#include <eagine/msgbus/service_requirements.hpp>
#include <eagine/signal_switch.hpp>
#include <eagine/timeout.hpp>
#include <algorithm>
#include <thread>
#include <vector>

namespace eagine {
namespace msgbus {
//------------------------------------------------------------------------------
template <typename Base = subscriber>
class data_provider_example
  : public main_ctx_object
  , public require_services<Base, stream_provider> {
    using base = require_services<Base, stream_provider>;

public:
    data_provider_example(endpoint& bus)
      : main_ctx_object{EAGINE_ID(Provider), bus}
      , base{bus} {
        this->stream_relay_assigned.connect(
          EAGINE_THIS_MEM_FUNC_REF(_handle_relay_assigned));
        this->stream_relay_reset.connect(
          EAGINE_THIS_MEM_FUNC_REF(_handle_relay_reset));

        _stream_ids.push_back([this] {
            msgbus::stream_info info{};
            info.kind = EAGINE_ID(Test);
            info.encoding = EAGINE_ID(Test);
            info.description = "Test stream 1";
            return this->add_stream(std::move(info));
        }());
    }

    auto is_done() const noexcept -> bool {
        return _done && _stream_ids.empty();
    }

protected:
    auto update() -> work_done {
        some_true something_done{base::update()};
        if(_done) {
            for(const auto id : _stream_ids) {
                this->remove_stream(id);
            }
            _stream_ids.clear();
            something_done();
        }
        return something_done;
    }

private:
    void _handle_relay_assigned(identifier_t relay_id) {
        log_info("stream relay ${relay} assigned")
          .arg(EAGINE_ID(relay), relay_id);
    }

    void _handle_relay_reset() {
        log_info("stream relay reset");
    }

    timeout _done{std::chrono::seconds{10}};
    std::vector<identifier_t> _stream_ids;
};
//------------------------------------------------------------------------------
template <typename Base = subscriber>
class data_consumer_example
  : public main_ctx_object
  , public require_services<Base, stream_consumer> {
    using base = require_services<Base, stream_consumer>;

public:
    data_consumer_example(endpoint& bus)
      : main_ctx_object{EAGINE_ID(Consumer), bus}
      , base{bus} {
        this->stream_relay_assigned.connect(
          EAGINE_THIS_MEM_FUNC_REF(_handle_relay_assigned));
        this->stream_appeared.connect(
          EAGINE_THIS_MEM_FUNC_REF(_handle_stream_appeared));
        this->stream_disappeared.connect(
          EAGINE_THIS_MEM_FUNC_REF(_handle_stream_disappeared));
    }

    auto is_done() const noexcept -> bool {
        return _had_streams && _current_streams.empty();
    }

private:
    void _handle_relay_assigned(identifier_t relay_id) {
        log_info("stream relay ${relay} assigned")
          .arg(EAGINE_ID(relay), relay_id);
    }

    void _handle_stream_appeared(
      identifier_t provider_id,
      const stream_info& info,
      msgbus::verification_bits) {
        log_info("stream ${stream} appeared at ${provider}")
          .arg(EAGINE_ID(provider), provider_id)
          .arg(EAGINE_ID(stream), info.id)
          .arg(EAGINE_ID(desc), info.description);
        _current_streams.insert({provider_id, info.id});
        _had_streams = true;
    }

    void _handle_stream_disappeared(
      identifier_t provider_id,
      const stream_info& info,
      msgbus::verification_bits) {
        log_info("stream ${stream} disappeared from ${provider}")
          .arg(EAGINE_ID(provider), provider_id)
          .arg(EAGINE_ID(stream), info.id)
          .arg(EAGINE_ID(desc), info.description);
        _current_streams.erase({provider_id, info.id});
    }

    flat_set<std::tuple<identifier_t, identifier_t>> _current_streams;
    bool _had_streams{false};
};
//------------------------------------------------------------------------------
} // namespace msgbus

auto main(main_ctx& ctx) -> int {
    signal_switch interrupted;
    enable_message_bus(ctx);
    msgbus::registry the_reg{ctx};

    auto& relay =
      the_reg.emplace<msgbus::service_composition<msgbus::stream_relay<>>>(
        EAGINE_ID(RelayEndpt));

    auto on_stream_announced = [&ctx](
                                 identifier_t provider_id,
                                 const msgbus::stream_info& info,
                                 msgbus::verification_bits) {
        ctx.log()
          .info("stream ${stream} announced by ${provider}")
          .arg(EAGINE_ID(provider), provider_id)
          .arg(EAGINE_ID(stream), info.id)
          .arg(EAGINE_ID(desc), info.description);
    };
    relay.stream_announced.connect({construct_from, on_stream_announced});

    auto on_stream_retracted = [&ctx](
                                 identifier_t provider_id,
                                 const msgbus::stream_info& info,
                                 msgbus::verification_bits) {
        ctx.log()
          .info("stream ${stream} retracted by ${provider}")
          .arg(EAGINE_ID(provider), provider_id)
          .arg(EAGINE_ID(stream), info.id)
          .arg(EAGINE_ID(desc), info.description);
    };
    relay.stream_retracted.connect({construct_from, on_stream_retracted});

    auto& provider =
      the_reg
        .emplace<msgbus::service_composition<msgbus::data_provider_example<>>>(
          EAGINE_ID(PrvdrEndpt));
    auto& consumer =
      the_reg
        .emplace<msgbus::service_composition<msgbus::data_consumer_example<>>>(
          EAGINE_ID(CnsmrEndpt));

    while(!interrupted && !(provider.is_done() && consumer.is_done())) {
        if(!the_reg.update_all()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
    }

    return 0;
}
//------------------------------------------------------------------------------
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    eagine::main_ctx_options options;
    options.app_id = EAGINE_ID(StreamExe);
    return eagine::main_impl(argc, argv, options);
}

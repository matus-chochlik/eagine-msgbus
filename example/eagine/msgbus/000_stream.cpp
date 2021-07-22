/// @example eagine/msgbus/000_stream.cpp
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
#include <eagine/main_ctx.hpp>
#include <eagine/main_fwd.hpp>
#include <eagine/message_bus.hpp>
#include <eagine/msgbus/registry.hpp>
#include <eagine/msgbus/service.hpp>
#include <eagine/msgbus/service/stream.hpp>
#include <eagine/msgbus/service_requirements.hpp>
#include <eagine/timeout.hpp>
#include <algorithm>
#include <thread>

namespace eagine {
namespace msgbus {
//------------------------------------------------------------------------------
using data_provider_base =
  service_composition<require_services<subscriber, stream_provider>>;

class data_provider_example
  : public main_ctx_object
  , public data_provider_base {
    using base = data_provider_base;

public:
    data_provider_example(endpoint& bus)
      : main_ctx_object{EAGINE_ID(Provider), bus}
      , base{bus} {}

    auto is_done() const noexcept -> bool {
        return false;
    }

private:
};
//------------------------------------------------------------------------------
using data_consumer_base =
  service_composition<require_services<subscriber, stream_consumer>>;

class data_consumer_example
  : public main_ctx_object
  , public data_consumer_base {
    using base = data_consumer_base;

public:
    data_consumer_example(endpoint& bus)
      : main_ctx_object{EAGINE_ID(Consumer), bus}
      , base{bus} {}

    auto is_done() const noexcept -> bool {
        return false;
    }

private:
};
//------------------------------------------------------------------------------
} // namespace msgbus

auto main(main_ctx& ctx) -> int {
    enable_message_bus(ctx);
    msgbus::registry the_reg{ctx};

    the_reg.emplace<msgbus::service_composition<msgbus::stream_relay<>>>(
      EAGINE_ID(RelayEndpt));
    auto& provider =
      the_reg.emplace<msgbus::data_provider_example>(EAGINE_ID(PrvdrEndpt));
    auto& consumer =
      the_reg.emplace<msgbus::data_consumer_example>(EAGINE_ID(CnsmrEndpt));

    while(!(provider.is_done() && consumer.is_done())) {
        if(!the_reg.update_all()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

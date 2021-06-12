/// @example eagine/message_bus/004_discovery.cpp
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
#include <eagine/main.hpp>
#include <eagine/message_bus.hpp>
#include <eagine/message_bus/service.hpp>
#include <eagine/message_bus/service/discovery.hpp>
#include <eagine/message_bus/service/shutdown.hpp>
#include <eagine/message_bus/service_requirements.hpp>
#include <eagine/signal_switch.hpp>
#include <thread>

namespace eagine {
namespace msgbus {
//------------------------------------------------------------------------------
using subscription_logger_base = service_composition<
  require_services<subscriber, subscriber_discovery, shutdown_target>>;

class subscription_logger
  : public main_ctx_object
  , public subscription_logger_base {
    using base = subscription_logger_base;

public:
    subscription_logger(endpoint& bus)
      : main_ctx_object{EAGINE_ID(SubscrLog), bus}
      , base{bus} {
        reported_alive.connect(EAGINE_THIS_MEM_FUNC_REF(is_alive));
        subscribed.connect(EAGINE_THIS_MEM_FUNC_REF(on_subscribed));
        unsubscribed.connect(EAGINE_THIS_MEM_FUNC_REF(on_unsubscribed));
        shutdown_requested.connect(EAGINE_THIS_MEM_FUNC_REF(on_shutdown));
    }

    void is_alive(const subscriber_info& info) {
        log_info("endpoint ${subscrbr} is alive")
          .arg(EAGINE_ID(subscrbr), info.endpoint_id);
    }

    void on_subscribed(const subscriber_info& info, message_id sub_msg) {
        log_info("endpoint ${subscrbr} subscribed to ${message}")
          .arg(EAGINE_ID(subscrbr), info.endpoint_id)
          .arg(EAGINE_ID(message), sub_msg);
        this->bus_node().query_certificate_of(info.endpoint_id);
    }

    void on_unsubscribed(const subscriber_info& info, message_id sub_msg) {
        log_info("endpoint ${subscrbr} unsubscribed from ${message}")
          .arg(EAGINE_ID(subscrbr), info.endpoint_id)
          .arg(EAGINE_ID(message), sub_msg);
    }

    void on_shutdown(
      std::chrono::milliseconds age,
      identifier_t subscriber_id,
      verification_bits verified) {
        log_info("received ${age} old shutdown request from ${subscrbr}")
          .arg(EAGINE_ID(age), age)
          .arg(EAGINE_ID(subscrbr), subscriber_id)
          .arg(EAGINE_ID(verified), verified);

        // TODO: verification
        if(age < std::chrono::seconds(2)) {
            _done = true;
        }
    }

    auto is_done() const noexcept -> bool {
        return _done;
    }

private:
    bool _done{false};
};
//------------------------------------------------------------------------------
} // namespace msgbus

auto main(main_ctx& ctx) -> int {

    signal_switch interrupted;

    msgbus::endpoint bus{EAGINE_ID(DiscoverEx), ctx};
    // TODO
    // bus.add_ca_certificate_pem(ca_certificate_pem(ctx));
    // bus.add_certificate_pem(msgbus_endpoint_certificate_pem(ctx));

    msgbus::subscription_logger sub_log{bus};

    ctx.bus().setup_connectors(sub_log);
    timeout waited_too_long{std::chrono::minutes(1)};

    while(!(interrupted || sub_log.is_done() || waited_too_long)) {
        sub_log.update();
        if(!sub_log.process_all()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }

    return 0;
}
//------------------------------------------------------------------------------
} // namespace eagine

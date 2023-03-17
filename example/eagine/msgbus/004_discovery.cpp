/// @example eagine/msgbus/004_discovery.cpp
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
      : main_ctx_object{"SubscrLog", bus}
      , base{bus} {
        connect<&subscription_logger::is_alive>(this, reported_alive);
        connect<&subscription_logger::on_subscribed>(this, subscribed);
        connect<&subscription_logger::on_unsubscribed>(this, unsubscribed);
        connect<&subscription_logger::on_shutdown>(this, shutdown_requested);
    }

    void is_alive(
      const result_context&,
      const subscriber_alive& alive) noexcept {
        log_info("endpoint ${subscrbr} is alive")
          .arg("subscrbr", alive.source.endpoint_id);
    }

    void on_subscribed(
      const result_context&,
      const subscriber_subscribed& sub) noexcept {
        log_info("endpoint ${subscrbr} subscribed to ${message}")
          .arg("subscrbr", sub.source.endpoint_id)
          .arg("message", sub.message_type);
        this->bus_node().query_certificate_of(sub.source.endpoint_id);
    }

    void on_unsubscribed(
      const result_context&,
      const subscriber_unsubscribed& sub) noexcept {
        log_info("endpoint ${subscrbr} unsubscribed from ${message}")
          .arg("subscrbr", sub.source.endpoint_id)
          .arg("message", sub.message_type);
    }

    void on_shutdown(
      const result_context&,
      const shutdown_request& req) noexcept {
        log_info("received ${age} old shutdown request from ${subscrbr}")
          .arg("age", req.age)
          .arg("subscrbr", req.source_id)
          .arg("verified", req.verified);

        // TODO: verification
        if(req.age < std::chrono::seconds(2)) {
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
    const signal_switch interrupted;
    enable_message_bus(ctx);

    msgbus::endpoint bus{"DiscoverEx", ctx};
    bus.add_ca_certificate_pem(ca_certificate_pem(ctx));
    bus.add_certificate_pem(msgbus::endpoint_certificate_pem(ctx));

    msgbus::subscription_logger sub_log{bus};

    msgbus::setup_connectors(ctx, sub_log);
    timeout waited_too_long{std::chrono::minutes(1)};

    while(not(interrupted or sub_log.is_done() or waited_too_long)) {
        sub_log.update();
        sub_log.process_all().or_sleep_for(std::chrono::milliseconds(250));
    }

    return 0;
}
//------------------------------------------------------------------------------
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    return eagine::default_main(argc, argv, eagine::main);
}


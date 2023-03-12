/// @example eagine/msgbus/003_shutdown.cpp
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
using shutdown_trigger_base = service_composition<
  require_services<subscriber, subscriber_discovery, shutdown_invoker>>;

class shutdown_trigger
  : public main_ctx_object
  , public shutdown_trigger_base {
    using base = shutdown_trigger_base;

public:
    shutdown_trigger(endpoint& bus)
      : main_ctx_object{"ShtdwnTrgr", bus}
      , base{bus} {
        subscribed.connect(make_callable_ref(
          this,
          member_function_constant_t<&shutdown_trigger::on_subscribed>{}));
        unsubscribed.connect(make_callable_ref(
          this,
          member_function_constant_t<&shutdown_trigger::on_unsubscribed>{}));
        not_subscribed.connect(make_callable_ref(
          this,
          member_function_constant_t<&shutdown_trigger::on_not_subscribed>{}));
    }

    void on_subscribed(const subscriber_subscribed& sub) noexcept {
        if(sub.message_type.is("Shutdown", "shutdown")) {
            log_info("target ${id} appeared").arg("id", sub.source.endpoint_id);
            _targets.insert(sub.source.endpoint_id);
            this->bus_node().post_certificate(sub.source.endpoint_id, 0);
        }
    }

    void on_unsubscribed(const subscriber_unsubscribed& sub) noexcept {
        if(sub.message_type.is("Shutdown", "shutdown")) {
            log_info("target ${id} disappeared")
              .arg("id", sub.source.endpoint_id);
            _targets.erase(sub.source.endpoint_id);
        }
    }

    void on_not_subscribed(const subscriber_not_subscribed& sub) noexcept {
        if(sub.message_type.is("Shutdown", "shutdown")) {
            log_info("target ${id} does not support shutdown")
              .arg("id", sub.source.endpoint_id);
            _targets.erase(sub.source.endpoint_id);
        }
    }

    void shutdown_all() {
        for(const auto id : _targets) {
            this->shutdown_one(id);
        }
    }

private:
    std::set<identifier_t> _targets{};
};
//------------------------------------------------------------------------------
} // namespace msgbus

auto main(main_ctx& ctx) -> int {
    enable_message_bus(ctx);

    msgbus::endpoint bus{"ShutdownEx", ctx};
    bus.add_ca_certificate_pem(ca_certificate_pem(ctx));
    bus.add_certificate_pem(msgbus::endpoint_certificate_pem(ctx));

    msgbus::shutdown_trigger trgr{bus};
    msgbus::setup_connectors(ctx, trgr);

    timeout wait_done{std::chrono::seconds(30)};

    while(not wait_done) {
        trgr.update();
        trgr.process_all().or_sleep_for(std::chrono::milliseconds(10));
    }

    trgr.shutdown_all();
    wait_done.reset();

    while(not wait_done) {
        trgr.update();
        trgr.process_all().or_sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
}
//------------------------------------------------------------------------------
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    return eagine::default_main(argc, argv, eagine::main);
}


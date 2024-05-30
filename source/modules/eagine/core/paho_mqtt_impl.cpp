/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>
#define PAHO_MQTT_IMPORTS 1
#include <MQTTClient.h>

module eagine.msgbus.core;

import std;
import eagine.core.debug;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.string;
import eagine.core.identifier;
import eagine.core.utility;
import eagine.core.runtime;
import eagine.core.valid_if;
import eagine.core.main_ctx;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
// Connection info
//------------------------------------------------------------------------------
template <typename Base>
class paho_mqtt_connection_info : public Base {
public:
    auto kind() noexcept -> connection_kind final {
        return connection_kind::remote_interprocess;
    }

    auto addr_kind() noexcept -> connection_addr_kind final {
        return connection_addr_kind::string;
    }

    auto type_id() noexcept -> identifier final {
        return "PahoMQTT";
    }
};
//------------------------------------------------------------------------------
class paho_mqtt_connection
  : public main_ctx_object
  , public paho_mqtt_connection_info<connection> {
public:
    paho_mqtt_connection(main_ctx_parent parent, const url& locator) noexcept;

    using fetch_handler = connection::fetch_handler;

    auto update() noexcept -> work_done final;

    void cleanup() noexcept final;

    auto is_usable() noexcept -> bool final;

    auto max_data_size() noexcept -> valid_if_positive<span_size_t> final;

    auto send(const message_id msg_id, const message_view&) noexcept
      -> bool final;

    auto fetch_messages(const fetch_handler handler) noexcept
      -> work_done final;

    auto query_statistics(connection_statistics&) noexcept -> bool final;

    auto routing_weight() noexcept -> float final;

private:
    auto _get_broker_url(const url&) noexcept -> std::string;
    auto _get_client_uid(const url&) noexcept -> std::string;

    auto _qos() const noexcept -> int;

    auto _add_subscription(string_view) noexcept -> bool;
    auto _remove_subscription(string_view) noexcept -> bool;

    void _message_delivered() noexcept;
    auto _message_arrived(string_view, memory::const_block) noexcept;
    void _connection_lost(string_view) noexcept;

    static void _message_delivered_f(void*, MQTTClient_deliveryToken);
    static auto _message_arrived_f(void*, char*, int, MQTTClient_message*)
      -> int;
    static void _connection_lost_f(void*, char*);

    const std::string _broker_url;
    const std::string _client_uid;

    ::MQTTClient _mqtt_client{};
    bool _created : 1 {false};
    bool _connected : 1 {false};
};
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_qos() const noexcept -> int {
    return 1;
}
//------------------------------------------------------------------------------
void paho_mqtt_connection::_message_delivered() noexcept {}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_message_arrived(
  string_view,
  memory::const_block) noexcept {}
//------------------------------------------------------------------------------
void paho_mqtt_connection::_connection_lost(string_view) noexcept {
    _connected = false;
}
//------------------------------------------------------------------------------
void paho_mqtt_connection::_message_delivered_f(
  void* context,
  MQTTClient_deliveryToken) {
    assert(context);
    auto* that{static_cast<paho_mqtt_connection*>(context)};
    that->_message_delivered();
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_message_arrived_f(
  void* context,
  char* topic_str,
  int topic_len,
  MQTTClient_message* message) -> int {
    assert(context);
    auto* that{static_cast<paho_mqtt_connection*>(context)};

    const auto topic_name{[&] -> string_view {
        if(topic_len > 0) {
            return {
              static_cast<const char*>(topic_str),
              static_cast<span_size_t>(topic_len)};
        }
        return string_view{topic_str};
    }};

    const auto content{[&] -> memory::const_block {
        if(message) {
            return {
              static_cast<const byte*>(message->payload),
              static_cast<span_size_t>(message->payloadlen)};
        }
        return {};
    }};

    that->_message_arrived(topic_name(), content());
    return 1;
}
//------------------------------------------------------------------------------
void paho_mqtt_connection::_connection_lost_f(void* context, char* reason) {
    assert(context);
    auto* that{static_cast<paho_mqtt_connection*>(context)};
    that->_connection_lost(string_view{reason});
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_add_subscription(string_view) noexcept -> bool {
    return false;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_remove_subscription(string_view) noexcept -> bool {
    return false;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_get_broker_url(const url& locator) noexcept
  -> std::string {
    return std::format(
      "tcp://{:s}:${:d}",
      locator.domain().value_or("localhost").std_span(),
      locator.port().value_or(1883));
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_get_client_uid(const url& locator) noexcept
  -> std::string {
    if(const auto uid{locator.login()}) {
        return *uid;
    }
    std::string result{'_', 16Z};
    main_context().random_identifier(result);
    return result;
}
//------------------------------------------------------------------------------
paho_mqtt_connection::paho_mqtt_connection(
  main_ctx_parent parent,
  const url& locator) noexcept
  : main_ctx_object{"PahoMQTTCn", parent}
  , _broker_url{_get_broker_url(locator)}
  , _client_uid{_get_client_uid(locator)} {}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::update() noexcept -> work_done {
    return {};
}
//------------------------------------------------------------------------------
void paho_mqtt_connection::cleanup() noexcept {}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::is_usable() noexcept -> bool {
    return _created and _connected;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::max_data_size() noexcept
  -> valid_if_positive<span_size_t> {
    return {4 * 1024};
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::send(
  const message_id msg_id,
  const message_view&) noexcept -> bool {
    return false;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::fetch_messages(const fetch_handler handler) noexcept
  -> work_done {
    (void)handler;
    some_true something_done;
    return something_done;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::query_statistics(connection_statistics&) noexcept
  -> bool {
    return false;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::routing_weight() noexcept -> float {
    return 1.F;
}
//------------------------------------------------------------------------------
// Factory
//------------------------------------------------------------------------------
class paho_mqtt_connection_factory
  : public paho_mqtt_connection_info<connection_factory>
  , public main_ctx_object {
public:
    using connection_factory::make_acceptor;
    using connection_factory::make_connector;

    paho_mqtt_connection_factory(main_ctx_parent parent) noexcept
      : main_ctx_object{"PahoConnFc", parent} {}

    auto make_acceptor(const string_view) noexcept
      -> shared_holder<acceptor> final;

    auto make_connector(const string_view addr_str) noexcept
      -> shared_holder<connection> final;

private:
};
//------------------------------------------------------------------------------
auto paho_mqtt_connection_factory::make_acceptor(const string_view) noexcept
  -> shared_holder<acceptor> {
    log_error("cannot create a PAHO MQTT connection acceptor.");
    return {};
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection_factory::make_connector(
  const string_view addr_str) noexcept -> shared_holder<connection> {
    return {hold<paho_mqtt_connection>, *this, url{to_string(addr_str)}};
}
//------------------------------------------------------------------------------
// Factory functions
//------------------------------------------------------------------------------
auto make_paho_mqtt_connection_factory(main_ctx_parent parent)
  -> unique_holder<connection_factory> {
    return {hold<paho_mqtt_connection_factory>, parent};
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


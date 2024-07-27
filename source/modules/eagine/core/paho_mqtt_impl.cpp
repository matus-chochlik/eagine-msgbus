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
import eagine.core.container;
import eagine.core.serialization;
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
    paho_mqtt_connection(main_ctx_parent parent, const url& locator);
    paho_mqtt_connection(paho_mqtt_connection&&) = delete;
    paho_mqtt_connection(const paho_mqtt_connection&) = delete;
    auto operator=(paho_mqtt_connection&&) = delete;
    auto operator=(const paho_mqtt_connection&) = delete;
    ~paho_mqtt_connection() noexcept;

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
    auto _get_client_uid(const url&) const noexcept -> identifier;
    auto _has_uid(const string_view uid) const noexcept -> bool;

    auto _qos() const noexcept -> int;

    void _add_subscription(string_view, bool) noexcept;
    void _remove_subscription(string_view) noexcept;
    auto _subscribe_to(string_view) noexcept -> bool;
    auto _unsubscribe_from(string_view) noexcept -> bool;

    auto _do_send(
      const string_view topic,
      const memory::const_block content) noexcept -> bool;

    void _message_delivered() noexcept;
    auto _message_arrived(string_view, memory::const_block) noexcept;
    void _connection_lost(string_view) noexcept;

    auto _topic_prefix() const noexcept -> string_view;
    auto _topic_to_msg_id(memory::span<const char> s) const noexcept
      -> std::tuple<message_id, endpoint_id_t>;
    auto _msg_id_to_subscr_topic(const message_id, endpoint_id_t, bool) noexcept
      -> string_view;
    auto _msg_id_to_topic(const message_id, endpoint_id_t) noexcept
      -> string_view;
    static void _message_delivered_f(void*, MQTTClient_deliveryToken);
    static auto _message_arrived_f(void*, char*, int, MQTTClient_message*)
      -> int;
    static void _connection_lost_f(void*, char*);

    auto _handle_req_id(const message_view&) noexcept
      -> message_handling_result;
    auto _handle_subsc(const message_view&) noexcept -> message_handling_result;
    auto _handle_unsub(const message_view&) noexcept -> message_handling_result;

    auto _handle_special_send(
      const message_id msg_id,
      const message_view&) noexcept -> message_handling_result;
    auto _handle_special_recv(
      const message_id msg_id,
      const message_view&) noexcept -> message_handling_result;

    const std::string _broker_url;
    identifier _client_uid;

    std::map<std::string, std::size_t, str_view_less> _subscriptions;
    memory::buffer_pool _buffers;
    std::string _temp_topic;
    memory::buffer _buffer;
    double_buffer<message_storage> _sent;
    double_buffer<message_storage> _received;
    std::mutex _send_mutex{};
    std::mutex _recv_mutex{};
    ::MQTTClient _mqtt_client{};
    bool _created : 1 {false};
    bool _connected : 1 {false};
};
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_qos() const noexcept -> int {
    return 0;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_has_uid(const string_view uid) const noexcept
  -> bool {
    return (string_view{"_"} == uid) or (_client_uid.name().view() == uid);
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_topic_prefix() const noexcept -> string_view {
    static const string_view topic_prefix{"eagi/bus/"};
    return topic_prefix;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_topic_to_msg_id(memory::span<const char> topic)
  const noexcept -> std::tuple<message_id, endpoint_id_t> {
    if(starts_with(topic, _topic_prefix())) {
        topic = skip(topic, _topic_prefix().size());
        const auto [cls_str, cls_tail]{split_by_first_element(topic, '/')};
        const auto [mth_str, mth_tail]{split_by_first_element(cls_tail, '/')};
        const auto [src_id, dst_id]{split_by_first_element(mth_tail, '/')};
        if(cls_str and mth_str and _has_uid(dst_id)) {
            if(identifier::can_be_encoded(cls_str)) {
                if(identifier::can_be_encoded(mth_str)) {
                    if(identifier::can_be_encoded(src_id)) {
                        return {
                          {identifier{cls_str}, identifier{mth_str}},
                          identifier{src_id}.value()};
                    }
                }
            }
        }
    }
    return {};
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_msg_id_to_subscr_topic(
  const message_id msg_id,
  endpoint_id_t source_id,
  bool broadcast) noexcept -> string_view {
    if(_temp_topic.empty()) [[unlikely]] {
        assign_to(_topic_prefix(), _temp_topic);
    } else {
        assert(starts_with(string_view{_temp_topic}, _topic_prefix()));
        _temp_topic.resize(std_size(_topic_prefix().size()));
    }
    append_to(msg_id.class_().name().view(), _temp_topic);
    _temp_topic.append("/");
    append_to(msg_id.method().name().view(), _temp_topic);
    if(broadcast) {
        _temp_topic.append("/+/_");
    } else {
        _temp_topic.append("/+/");
        append_to(identifier{source_id.value()}.name().view(), _temp_topic);
    }
    return {_temp_topic};
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_msg_id_to_topic(
  const message_id msg_id,
  endpoint_id_t target_id) noexcept -> string_view {
    if(_temp_topic.empty()) [[unlikely]] {
        assign_to(_topic_prefix(), _temp_topic);
    } else {
        assert(starts_with(string_view{_temp_topic}, _topic_prefix()));
        _temp_topic.resize(std_size(_topic_prefix().size()));
    }
    append_to(msg_id.class_().name().view(), _temp_topic);
    _temp_topic.append("/");
    append_to(msg_id.method().name().view(), _temp_topic);
    _temp_topic.append("/");
    append_to(_client_uid.name().view(), _temp_topic);
    if(target_id) {
        _temp_topic.append("/");
        append_to(identifier{target_id.value()}.name().view(), _temp_topic);
    } else {
        _temp_topic.append("/_");
    }
    return {_temp_topic};
}
//------------------------------------------------------------------------------
void paho_mqtt_connection::_message_delivered() noexcept {
    // TODO: some stats?
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_message_arrived(
  string_view topic,
  memory::const_block data) noexcept {
    const auto [msg_id, src_id]{_topic_to_msg_id(topic)};
    if(msg_id) [[likely]] {
        if(_client_uid.value() != src_id) {
            if(not _handle_special_recv(msg_id, data)) {
                const std::unique_lock lock{_recv_mutex};
                block_data_source source(data);
                default_deserializer_backend backend(source);
                message_id msg_id{};
                stored_message message{};
                if(deserialize_message(msg_id, message, backend)) [[likely]] {
                    _received.next().push(msg_id, message);
                } else {
                    log_error("failed to deserialize message")
                      .arg("size", data.size());
                }
            }
        }
    }
}
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
void paho_mqtt_connection::_add_subscription(
  string_view topic,
  bool success) noexcept {
    auto pos{_subscriptions.find(topic)};
    if(pos == _subscriptions.end()) {
        pos = std::get<0>(_subscriptions.emplace(to_string(topic), 0Z));
    }
    std::get<1>(*pos) = success;
}
//------------------------------------------------------------------------------
void paho_mqtt_connection::_remove_subscription(string_view topic) noexcept {
    auto pos{_subscriptions.find(topic)};
    if(pos != _subscriptions.end()) {
        _subscriptions.erase(pos);
    }
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_subscribe_to(string_view topic) noexcept -> bool {
    if(is_usable()) {
        if(
          MQTTClient_subscribe(_mqtt_client, c_str(topic), 1) ==
          MQTTCLIENT_SUCCESS) {
            log_info("${client} subscribes to ${topic}")
              .arg("client", _client_uid)
              .arg("topic", topic);
            return true;
        }
    }
    return false;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_unsubscribe_from(string_view topic) noexcept
  -> bool {
    if(is_usable()) {
        if(
          MQTTClient_unsubscribe(_mqtt_client, c_str(topic)) ==
          MQTTCLIENT_SUCCESS) {
            log_info("${client} unsubscribes from ${topic}")
              .arg("client", _client_uid)
              .arg("topic", topic);
            return true;
        }
    }
    return false;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_get_broker_url(const url& locator) noexcept
  -> std::string {
    return std::format(
      "tcp://{:s}:{:d}",
      locator.domain().value_or("localhost").std_span(),
      locator.port().value_or(1883));
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_get_client_uid(const url& locator) const noexcept
  -> identifier {
    if(const auto uid{locator.login()}) {
        if(identifier::can_be_encoded(*uid)) {
            return identifier{string_view{*uid}};
        }
    }
    return main_context().random_identifier();
}
//------------------------------------------------------------------------------
paho_mqtt_connection::paho_mqtt_connection(
  main_ctx_parent parent,
  const url& locator)
  : main_ctx_object{"PahoMQTTCn", parent}
  , _broker_url{_get_broker_url(locator)}
  , _client_uid{_get_client_uid(locator)} {
    _buffer.resize(4 * 1024);
    if(
      MQTTClient_create(
        &_mqtt_client,
        _broker_url.c_str(),
        _client_uid.name().str().c_str(),
        MQTTCLIENT_PERSISTENCE_NONE,
        nullptr) != MQTTCLIENT_SUCCESS) {

        log_error("PAHO MQTT client creation failed (${clientUrl})")
          .arg("clientUrl", _broker_url)
          .arg("clientUid", _client_uid);
        throw std::runtime_error("failed to create MQTT client");
    }
    _created = true;

    if(
      MQTTClient_setCallbacks(
        _mqtt_client,
        static_cast<void*>(this),
        _connection_lost_f,
        _message_arrived_f,
        _message_delivered_f) != MQTTCLIENT_SUCCESS) {

        log_error("PAHO MQTT client set callbacks failed (${clientUrl})")
          .arg("clientUrl", _broker_url)
          .arg("clientUid", _client_uid);
        throw std::runtime_error("failed to set MQTT client callbacks");
    }

    MQTTClient_connectOptions paho_opts = MQTTClient_connectOptions_initializer;
    paho_opts.keepAliveInterval = 10;
    paho_opts.cleansession = 1;
    if(MQTTClient_connect(_mqtt_client, &paho_opts) != MQTTCLIENT_SUCCESS) {

        log_error("PAHO MQTT client connection failed (${clientUrl})")
          .arg("clientUrl", _broker_url)
          .arg("clientUid", _client_uid);
        throw std::runtime_error("failed to connect MQTT client");
    }
    _connected = true;

    log_info("PAHO MQTT created: ${clientUrl}")
      .arg("clientUrl", _broker_url)
      .arg("clientUid", _client_uid);
}
//------------------------------------------------------------------------------
paho_mqtt_connection::~paho_mqtt_connection() noexcept {
    cleanup();
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_do_send(
  const string_view topic,
  const memory::const_block content) noexcept -> bool {
    if(not is_usable()) {
        return false;
    }
    return MQTTClient_publish(
             _mqtt_client,
             c_str(topic),
             static_cast<int>(content.size()),
             static_cast<const void*>(content.data()),
             _qos(),
             0,
             nullptr) == MQTTCLIENT_SUCCESS;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::update() noexcept -> work_done {
    // TODO: update
    const auto handler{[this](
                         const message_id msg_id,
                         const message_age,
                         const message_view& message) {
        block_data_sink sink(cover(_buffer));
        default_serializer_backend backend(sink);
        if(serialize_message(msg_id, message, backend)) [[likely]] {
            return _do_send(
              _msg_id_to_topic(msg_id, message.target_id), sink.done());
        }
        return false;
    }};
    auto& sent{[this] -> message_storage& {
        const std::unique_lock lock{_send_mutex};
        _sent.swap();
        return _sent.current();
    }()};
    return sent.fetch_all({construct_from, handler});
}
//------------------------------------------------------------------------------
void paho_mqtt_connection::cleanup() noexcept {
    if(_connected) {
        _connected = false;
        MQTTClient_disconnect(_mqtt_client, 100);
    }
    if(_created) {
        _created = false;
        MQTTClient_destroy(&_mqtt_client);
    }
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::is_usable() noexcept -> bool {
    return _created and _connected;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::max_data_size() noexcept
  -> valid_if_positive<span_size_t> {
    return {_buffer.size()};
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_handle_req_id(const message_view&) noexcept
  -> message_handling_result {
    // send the special message assigning the endpoint id
    message_view response{};
    response.set_source_id(0);
    response.set_target_id(_client_uid.value());
    const std::unique_lock lock{_recv_mutex};
    _received.next().push(msgbus_id{"assignId"}, response);
    return was_handled;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_handle_subsc(const message_view& message) noexcept
  -> message_handling_result {
    message_id sub_msg_id{};
    if(default_deserialize_message_type(sub_msg_id, message.content()))
      [[likely]] {
        const std::unique_lock lock{_send_mutex};
        for(const auto broadcast : {true, false}) {
            const auto topic{_msg_id_to_subscr_topic(
              sub_msg_id, message.source_id, broadcast)};
            _add_subscription(topic, _subscribe_to(topic));
        }
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_handle_unsub(const message_view& message) noexcept
  -> message_handling_result {
    message_id sub_msg_id{};
    if(default_deserialize_message_type(sub_msg_id, message.content()))
      [[likely]] {
        const std::unique_lock lock{_send_mutex};
        for(const auto broadcast : {true, false}) {
            const auto topic{_msg_id_to_subscr_topic(
              sub_msg_id, message.source_id, broadcast)};
            if(_unsubscribe_from(topic)) {
                _remove_subscription(topic);
            }
        }
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_handle_special_send(
  const message_id msg_id,
  const message_view& message) noexcept -> message_handling_result {
    if(is_special_message(msg_id)) {
        switch(msg_id.method_id()) {
            case id_v("requestId"):
                return _handle_req_id(message);
            case id_v("subscribTo"):
                return _handle_subsc(message);
            case id_v("unsubFrom"):
                return _handle_unsub(message);
            case id_v("byeByeEndp"):
            case id_v("byeByeRutr"):
            case id_v("byeByeBrdg"):
            case id_v("msgFlowInf"):
            case id_v("annEndptId"):
                return was_handled;
            default:;
        }
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::_handle_special_recv(
  const message_id msg_id,
  const message_view& message) noexcept -> message_handling_result {
    if(is_special_message(msg_id)) {
        switch(msg_id.method_id()) {
            default:
                break;
        }
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::send(
  const message_id msg_id,
  const message_view& content) noexcept -> bool {
    if(is_special_message(msg_id)) {
        if(_handle_special_send(msg_id, content)) {
            return true;
        }
    }
    const std::unique_lock lock{_send_mutex};
    _sent.next().push(msg_id, content);
    return true;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::fetch_messages(const fetch_handler handler) noexcept
  -> work_done {
    auto& received{[this] -> message_storage& {
        const std::unique_lock lock{_recv_mutex};
        _received.swap();
        return _received.current();
    }()};
    return received.fetch_all(handler);
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
    try {
        return {hold<paho_mqtt_connection_factory>, parent};
    } catch(const std::exception&) {
        return {};
    }
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


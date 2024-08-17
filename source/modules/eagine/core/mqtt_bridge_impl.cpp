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
import eagine.core.build_config;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.string;
import eagine.core.identifier;
import eagine.core.serialization;
import eagine.core.utility;
import eagine.core.valid_if;
import eagine.core.runtime;
import eagine.core.main_ctx;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
// mqtt_bridge_state
//------------------------------------------------------------------------------
class mqtt_bridge_state : public main_ctx_object {
public:
    mqtt_bridge_state(
      main_ctx_parent parent,
      const url& locator,
      const valid_if_positive<span_size_t>&);
    mqtt_bridge_state(mqtt_bridge_state&&) = delete;
    mqtt_bridge_state(const mqtt_bridge_state&) = delete;
    auto operator=(mqtt_bridge_state&&) = delete;
    auto operator=(const mqtt_bridge_state&) = delete;
    ~mqtt_bridge_state() noexcept;

    auto is_usable() const noexcept -> bool;

    void push(const message_id msg_id, const message_view& message) noexcept;

    auto forwarded_messages() const noexcept {
        return _forwarded_messages;
    }

    auto dropped_messages() const noexcept {
        return _dropped_messages;
    }

    auto decode_errors() const noexcept {
        return _decode_errors;
    }

    void send_to_mqtt() noexcept;

    using fetch_handler = message_storage::fetch_handler;

    auto fetch_messages(const fetch_handler handler) noexcept;

    void recv_from_mqtt() noexcept;

private:
    auto _qos() const noexcept -> int;
    auto _get_broker_url(const url&) noexcept -> std::string;
    auto _get_client_uid(const url&) const noexcept -> identifier;
    auto _subscribe_to(string_view) noexcept -> bool;
    auto _unsubscribe_from(string_view) noexcept -> bool;

    auto _unpack_message(string_view, memory::const_block) noexcept
      -> std::tuple<message_id, identifier_t, memory::const_block>;

    static void _message_delivered_f(void*, MQTTClient_deliveryToken);
    static auto _message_arrived_f(void*, char*, int, MQTTClient_message*)
      -> int;
    static void _connection_lost_f(void*, char*);

    void _message_delivered() noexcept;
    auto _message_arrived(string_view, memory::const_block) noexcept;
    void _connection_lost(string_view) noexcept;

    span_size_t _forwarded_messages{0};
    span_size_t _dropped_messages{0};
    span_size_t _decode_errors{0};

    const std::string _broker_url;
    identifier _client_uid;

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
auto mqtt_bridge_state::_qos() const noexcept -> int {
    return 0;
}
//------------------------------------------------------------------------------
auto mqtt_bridge_state::_get_broker_url(const url& locator) noexcept
  -> std::string {
    return std::format(
      "tcp://{:s}:{:d}",
      locator.domain().value_or("localhost").std_span(),
      locator.port().value_or(1883));
}
//------------------------------------------------------------------------------
auto mqtt_bridge_state::_get_client_uid(const url& locator) const noexcept
  -> identifier {
    if(const auto uid{locator.login()}) {
        if(identifier::can_be_encoded(*uid)) {
            return identifier{string_view{*uid}};
        }
    }
    return main_context().random_identifier();
}
//------------------------------------------------------------------------------
auto mqtt_bridge_state::_subscribe_to(string_view topic) noexcept -> bool {
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
auto mqtt_bridge_state::_unsubscribe_from(string_view topic) noexcept -> bool {
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
void mqtt_bridge_state::_message_delivered() noexcept {
    // TODO: some stats?
}
//------------------------------------------------------------------------------
auto mqtt_bridge_state::_unpack_message(
  string_view,
  memory::const_block) noexcept
  -> std::tuple<message_id, identifier_t, memory::const_block> {
    // TODO
    return {};
}
//------------------------------------------------------------------------------
auto mqtt_bridge_state::_message_arrived(
  string_view topic,
  memory::const_block payload) noexcept {
    const auto [msg_id, src_id, data]{_unpack_message(topic, payload)};
    if(msg_id) [[likely]] {
        if(_client_uid.value() != src_id) {
            block_data_source source(data);
            default_deserializer_backend backend(source);
            message_id msg_id{};
            stored_message message{};
            if(deserialize_message(msg_id, message, backend)) [[likely]] {
                const std::unique_lock lock{_recv_mutex};
                _received.next().push(msg_id, message);
            } else {
                log_error("failed to deserialize message")
                  .arg("size", data.size());
            }
        }
    }
}
//------------------------------------------------------------------------------
void mqtt_bridge_state::_connection_lost(string_view) noexcept {
    _connected = false;
}
//------------------------------------------------------------------------------
void mqtt_bridge_state::_message_delivered_f(
  void* context,
  MQTTClient_deliveryToken) {
    assert(context);
    auto* that{static_cast<mqtt_bridge_state*>(context)};
    that->_message_delivered();
}
//------------------------------------------------------------------------------
auto mqtt_bridge_state::_message_arrived_f(
  void* context,
  char* topic_str,
  int topic_len,
  MQTTClient_message* message) -> int {
    assert(context);
    auto* that{static_cast<mqtt_bridge_state*>(context)};

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
void mqtt_bridge_state::_connection_lost_f(void* context, char* reason) {
    assert(context);
    auto* that{static_cast<mqtt_bridge_state*>(context)};
    that->_connection_lost(string_view{reason});
}
//------------------------------------------------------------------------------
mqtt_bridge_state::mqtt_bridge_state(
  main_ctx_parent parent,
  const url& locator,
  const valid_if_positive<span_size_t>&)
  : main_ctx_object{"mqttBrgSte", parent}
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
mqtt_bridge_state::~mqtt_bridge_state() noexcept {
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
auto mqtt_bridge_state::is_usable() const noexcept -> bool {
    return _created and _connected;
}

//------------------------------------------------------------------------------
void mqtt_bridge_state::push(
  const message_id msg_id,
  const message_view& message) noexcept {
    const std::unique_lock lock{_send_mutex};
    _sent.next().push(msg_id, message);
}
//------------------------------------------------------------------------------
auto mqtt_bridge_state::fetch_messages(const fetch_handler handler) noexcept {
    auto& queue{[this]() -> message_storage& {
        const std::unique_lock lock{_recv_mutex};
        _received.swap();
        return _received.current();
    }()};
    return queue.fetch_all(handler);
}
//------------------------------------------------------------------------------
void mqtt_bridge_state::recv_from_mqtt() noexcept {
    // TODO
}
//------------------------------------------------------------------------------
void mqtt_bridge_state::send_to_mqtt() noexcept {
    // TODO
}
//------------------------------------------------------------------------------
// MQTT bridge
//------------------------------------------------------------------------------
mqtt_bridge::mqtt_bridge(main_ctx_parent parent) noexcept
  : main_ctx_object("BusMqttBrg", parent)
  , _context{make_context(*this)}
  , _instance_id{process_instance_id()}
  , _no_id_timeout{adjusted_duration(std::chrono::seconds{2}), nothing}
  , _startup_time{std::chrono::steady_clock::now()}
  , _forwarded_since_m2c{std::chrono::steady_clock::now()}
  , _forwarded_since_c2m{std::chrono::steady_clock::now()}
  , _forwarded_since_stat{std::chrono::steady_clock::now()} {
    _setup_from_config();
}
//------------------------------------------------------------------------------
auto mqtt_bridge::_uptime_seconds() noexcept -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::steady_clock::now() - _startup_time)
      .count();
}
//------------------------------------------------------------------------------
void mqtt_bridge::add_certificate_pem(const memory::const_block blk) noexcept {
    if(_context) {
        _context->add_own_certificate_pem(blk);
    }
}
//------------------------------------------------------------------------------
void mqtt_bridge::add_ca_certificate_pem(
  const memory::const_block blk) noexcept {
    if(_context) {
        _context->add_ca_certificate_pem(blk);
    }
}
//------------------------------------------------------------------------------
auto mqtt_bridge::add_connection(shared_holder<connection> conn) noexcept
  -> bool {
    _connection = std::move(conn);
    return true;
}
//------------------------------------------------------------------------------
void mqtt_bridge::_setup_from_config() {
    auto& config{main_context().config()};
    if(config.fetch("msgbus.bridge.mqtt_broker", _broker_url)) {
        log_info("using MQTT broker URL ${url}")
          .arg("url", _broker_url.get_string());
    }
    // TODO: use app_config() to setup the connection
}
//------------------------------------------------------------------------------
auto mqtt_bridge::_handle_id_assigned(const message_view& message) noexcept
  -> message_handling_result {
    if(not has_id()) {
        _id = message.target_id;
        log_debug("assigned bridge id ${id} by router").arg("id", _id);
    }
    return was_handled;
}
//------------------------------------------------------------------------------
auto mqtt_bridge::_handle_id_confirmed(const message_view& message) noexcept
  -> message_handling_result {
    if(has_id()) {
        if(_id != message.target_id) {
            log_error("mismatching current and confirmed ids")
              .arg("current", _id)
              .arg("confirmed", message.target_id);
        }
    } else {
        log_warning("confirming unset id ${newId}")
          .arg("confirmed", message.target_id);
    }
    return was_handled;
}
//------------------------------------------------------------------------------
auto mqtt_bridge::_handle_ping(
  const message_view& message,
  const bool to_connection) noexcept -> message_handling_result {
    if(has_id()) [[likely]] {
        if(_id == message.target_id) {
            message_view response{};
            response.setup_response(message);
            response.set_source_id(_id);
            if(to_connection) {
                _do_push(msgbus_id{"pong"}, response);
            } else {
                _send(msgbus_id{"pong"}, response);
            }
            return was_handled;
        }
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto mqtt_bridge::_handle_topo_bridge_conn(
  const message_view& message,
  const bool to_connection) noexcept -> message_handling_result {
    if(to_connection) {
        bridge_topology_info info{};
        if(default_deserialize(info, message.content())) [[likely]] {
            info.opposite_id = _id;
            auto temp{default_serialize_buffer_for(info)};
            if(auto serialized{default_serialize(info, cover(temp))}) {
                message_view response{message, *serialized};
                _send(msgbus_id{"topoBrdgCn"}, response);
                return was_handled;
            }
        }
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto mqtt_bridge::_handle_topology_query(
  const message_view& message,
  const bool to_connection) noexcept -> message_handling_result {
    bridge_topology_info info{};
    info.bridge_id = _id;
    info.instance_id = _instance_id;
    auto temp{default_serialize_buffer_for(info)};
    if(const auto serialized{default_serialize(info, cover(temp))}) [[likely]] {
        message_view response{*serialized};
        response.setup_response(message);
        if(to_connection) {
            _do_push(msgbus_id{"topoBrdgCn"}, response);
        } else {
            _send(msgbus_id{"topoBrdgCn"}, response);
        }
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto mqtt_bridge::_handle_stats_query(
  const message_view& message,
  const bool to_connection) noexcept -> message_handling_result {
    _stats.forwarded_messages = _forwarded_messages_m2c;
    _stats.dropped_messages = _dropped_messages_m2c;
    _stats.uptime_seconds = _uptime_seconds();

    const auto now{std::chrono::steady_clock::now()};
    const std::chrono::duration<float> seconds{now - _forwarded_since_stat};
    if(seconds.count() > 15.F) [[likely]] {
        _forwarded_since_stat = now;

        _stats.messages_per_second = static_cast<std::int32_t>(
          float(_stats.forwarded_messages - _prev_forwarded_messages) /
          seconds.count());
        _prev_forwarded_messages = _stats.forwarded_messages;
    }

    auto bs_buf{default_serialize_buffer_for(_stats)};
    if(const auto serialized{default_serialize(_stats, cover(bs_buf))}) {
        message_view response{*serialized};
        response.setup_response(message);
        response.set_source_id(_id);
        if(to_connection) {
            _do_push(msgbus_id{"statsBrdg"}, response);
        } else {
            _send(msgbus_id{"statsBrdg"}, response);
        }
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto mqtt_bridge::_handle_special(
  const message_id msg_id,
  const message_view& message,
  const bool to_connection) noexcept -> message_handling_result {
    if(is_special_message(msg_id)) [[unlikely]] {
        log_debug("bridge handling special message ${message}")
          .tag("hndlSpcMsg")
          .arg("bridge", _id)
          .arg("message", msg_id)
          .arg("target", message.target_id)
          .arg("source", message.source_id);

        switch(msg_id.method_id()) {
            case id_v("assignId"):
                return _handle_id_assigned(message);
            case id_v("confirmId"):
                return _handle_id_confirmed(message);
            case id_v("ping"):
                return _handle_ping(message, to_connection);
            case id_v("topoBrdgCn"):
                return _handle_topo_bridge_conn(message, to_connection);
            case id_v("topoQuery"):
                return _handle_topology_query(message, to_connection);
            case id_v("statsQuery"):
                return _handle_stats_query(message, to_connection);
            case id_v("msgFlowInf"):
                return was_handled;
            default:
                break;
        }
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto mqtt_bridge::_do_send(
  const message_id msg_id,
  message_view& message) noexcept -> bool {
    message.add_hop();
    if(_connection) [[likely]] {
        if(_connection->send(msg_id, message)) {
            log_trace("forwarding message ${message} to connection")
              .arg("message", msg_id)
              .arg("data", message.data());
            return true;
        }
    }
    return false;
}
//------------------------------------------------------------------------------
auto mqtt_bridge::_send(const message_id msg_id, message_view& message) noexcept
  -> bool {
    assert(has_id());
    message.set_source_id(_id);
    return _do_send(msg_id, message);
}
//------------------------------------------------------------------------------
auto mqtt_bridge::_do_push(
  const message_id msg_id,
  message_view& message) noexcept -> bool {
    if(_state) [[likely]] {
        message.add_hop();
        _state->push(msg_id, message);
        log_trace("forwarding message ${message} to MQTT")
          .arg("message", msg_id)
          .arg("data", message.data());
        return true;
    }
    return false;
}
//------------------------------------------------------------------------------
auto mqtt_bridge::_avg_msg_age_c2m() const noexcept
  -> std::chrono::microseconds {
    return std::chrono::duration_cast<std::chrono::microseconds>(
      _message_age_sum_c2m /
      (_forwarded_messages_c2m + _dropped_messages_c2m + 1));
}
//------------------------------------------------------------------------------
auto mqtt_bridge::_avg_msg_age_m2c() const noexcept
  -> std::chrono::microseconds {
    return std::chrono::duration_cast<std::chrono::microseconds>(
      _message_age_sum_m2c /
      (_forwarded_messages_m2c + _dropped_messages_m2c + 1));
}
//------------------------------------------------------------------------------
constexpr auto bridge_log_stat_msg_count() noexcept {
    return debug_build ? 100'000 : 1'000'000;
}
//------------------------------------------------------------------------------
auto mqtt_bridge::_should_log_bridge_stats_c2m() noexcept -> bool {
    return ++_forwarded_messages_c2m % bridge_log_stat_msg_count() == 0;
}
//------------------------------------------------------------------------------
auto mqtt_bridge::_should_log_bridge_stats_m2c() noexcept -> bool {
    return ++_forwarded_messages_m2c % bridge_log_stat_msg_count() == 0;
}
//------------------------------------------------------------------------------
void mqtt_bridge::_log_bridge_stats_c2m() noexcept {
    const auto now{std::chrono::steady_clock::now()};
    const std::chrono::duration<float> interval{now - _forwarded_since_c2m};

    if(interval > decltype(interval)::zero()) [[likely]] {
        const auto msgs_per_sec{
          float(bridge_log_stat_msg_count()) / interval.count()};

        log_chart_sample("msgPerSecO", msgs_per_sec);
        log_stat("forwarded ${count} messages to output (${msgsPerSec})")
          .tag("msgStats")
          .arg("count", _forwarded_messages_c2m)
          .arg("dropped", _dropped_messages_c2m)
          .arg("interval", interval)
          .arg("avgMsgAge", _avg_msg_age_c2m())
          .arg("msgsPerSec", "RatePerSec", msgs_per_sec);
    }

    _forwarded_since_c2m = now;
}
//------------------------------------------------------------------------------
void mqtt_bridge::_log_bridge_stats_m2c() noexcept {
    const auto now{std::chrono::steady_clock::now()};
    const std::chrono::duration<float> interval{now - _forwarded_since_m2c};

    if(interval > decltype(interval)::zero()) [[likely]] {
        const auto msgs_per_sec{
          float(bridge_log_stat_msg_count()) / interval.count()};

        _stats.message_age_milliseconds =
          std::chrono::duration_cast<std::chrono::milliseconds>(
            _avg_msg_age_m2c())
            .count();

        log_chart_sample("msgPerSecI", msgs_per_sec);
        log_stat("forwarded ${count} messages from MQTT (${msgsPerSec})")
          .tag("msgStats")
          .arg("count", _forwarded_messages_m2c)
          .arg("dropped", _dropped_messages_m2c)
          .arg("interval", interval)
          .arg("avgMsgAge", _avg_msg_age_m2c())
          .arg("msgsPerSec", "RatePerSec", msgs_per_sec);
    }

    _forwarded_since_m2c = now;
}
//------------------------------------------------------------------------------
auto mqtt_bridge::_forward_messages() noexcept -> work_done {
    some_true something_done{};

    const auto forward_conn_to_mqtt{
      [this](
        const message_id msg_id, message_age msg_age, message_view message) {
          _message_age_sum_c2m += message.add_age(msg_age).age();
          if(message.too_old()) [[unlikely]] {
              ++_dropped_messages_c2m;
              return true;
          }
          if(_should_log_bridge_stats_c2m()) [[unlikely]] {
              _log_bridge_stats_c2m();
          }
          if(_handle_special(msg_id, message, false) == was_handled) {
              return true;
          }
          return this->_do_push(msg_id, message);
      }};

    if(_connection) [[likely]] {
        something_done(
          _connection->fetch_messages({construct_from, forward_conn_to_mqtt}));
    }

    if(_state) [[likely]] {
        const auto forward_mqtt_to_conn{[this](
                                          const message_id msg_id,
                                          message_age msg_age,
                                          message_view message) {
            _message_age_sum_m2c += message.add_age(msg_age).age();
            if(message.too_old()) [[unlikely]] {
                ++_dropped_messages_m2c;
                return true;
            }
            if(_should_log_bridge_stats_m2c()) [[unlikely]] {
                _log_bridge_stats_m2c();
            }
            if(this->_handle_special(msg_id, message, true) == was_handled) {
                return true;
            }
            this->_do_send(msg_id, message);
            return true;
        }};

        something_done(
          _state->fetch_messages({construct_from, forward_mqtt_to_conn}));
    }

    return something_done;
}
//------------------------------------------------------------------------------

auto mqtt_bridge::_check_state() noexcept -> work_done {
    some_true something_done{};

    if(not(_state and _state->is_usable())) [[unlikely]] {
        if(_connection) {
            if(const auto max_data_size{_connection->max_data_size()}) {
                ++_state_count;
                try {
                    _state.emplace(as_parent(), _broker_url, max_data_size);
                } catch(...) {
                }
                something_done();
            }
        }
    }

    return something_done;
}
//------------------------------------------------------------------------------
auto mqtt_bridge::_update_connections() noexcept -> work_done {
    some_true something_done{};

    if(_connection) [[likely]] {
        if(not has_id() and _no_id_timeout) [[unlikely]] {
            log_debug("requesting bridge id");
            _connection->send(msgbus_id{"requestId"}, {});
            _no_id_timeout.reset();
            something_done();
        }
        if(_connection->update()) {
            something_done();
            _no_connection_timeout.reset();
        }
    }
    return something_done;
}
//------------------------------------------------------------------------------
auto mqtt_bridge::update() noexcept -> work_done {
    static const auto exec_time_id{register_time_interval("busUpdate")};
    const auto exec_time{measure_time_interval(exec_time_id)};
    some_true something_done{};

    const bool had_id = has_id();
    something_done(_check_state());
    something_done(_update_connections());
    something_done(_forward_messages());

    // if processing the messages assigned the id
    if(has_id() and not had_id) [[unlikely]] {
        log_debug("announcing id ${id}").arg("id", _id);
        message_view msg;
        _send(msgbus_id{"announceId"}, msg);
        something_done();
    }

    return something_done;
}
//------------------------------------------------------------------------------
auto mqtt_bridge::is_done() const noexcept -> bool {
    return no_connection_timeout().is_expired();
}
//------------------------------------------------------------------------------
void mqtt_bridge::say_bye() noexcept {
    const auto msgid = msgbus_id{"byeByeBrdg"};
    message_view msg{};
    msg.set_source_id(_id);
    if(_connection) {
        _connection->send(msgid, msg);
        _connection->update();
    }
    if(_state) {
        _do_push(msgid, msg);
        std::this_thread::sleep_for(std::chrono::seconds{1});
    }
    _forward_messages();
    _update_connections();
}
//------------------------------------------------------------------------------
void mqtt_bridge::cleanup() noexcept {
    if(_connection) {
        _connection->cleanup();
    }
    const auto avg_msg_age_c2m{
      _message_age_sum_c2m /
      float(_forwarded_messages_c2m + _dropped_messages_c2m + 1)};
    const auto avg_msg_age_m2c{
      _message_age_sum_m2c /
      float(_forwarded_messages_m2c + _dropped_messages_m2c + 1)};

    if(_state) {
        log_stat("forwarded ${count} messages in total to output stream")
          .tag("msgStats")
          .arg("count", _state->forwarded_messages())
          .arg("dropped", _state->dropped_messages())
          .arg("decodeErr", _state->decode_errors())
          .arg("stateCount", _state_count);
    }

    log_stat("forwarded ${count} messages in total to output queue")
      .tag("msgStats")
      .arg("count", _forwarded_messages_c2m)
      .arg("dropped", _dropped_messages_c2m)
      .arg("avgMsgAge", avg_msg_age_c2m);

    log_stat("forwarded ${count} messages in total to connection")
      .tag("msgStats")
      .arg("count", _forwarded_messages_m2c)
      .arg("dropped", _dropped_messages_m2c)
      .arg("avgMsgAge", avg_msg_age_m2c);
}
//------------------------------------------------------------------------------
void mqtt_bridge::finish() noexcept {
    say_bye();
    timeout too_long{adjusted_duration(std::chrono::seconds{1})};
    while(not too_long) {
        update();
    }
    cleanup();
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

module eagine.msgbus.core;

import std;
import eagine.core.build_config;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.container;
import eagine.core.utility;
import eagine.core.valid_if;
import eagine.core.main_ctx;
import :types;
import :blobs;
import :message;
import :context;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
// endpoint
//------------------------------------------------------------------------------
auto endpoint::_default_store_handler() noexcept -> fetch_handler {
    return make_callable_ref<&endpoint::_store_message>(this);
}
//------------------------------------------------------------------------------
auto endpoint::_declare_states() noexcept {
    declare_state("msgCongest", "msgAgeHigh", "msgAgeNorm");
}
//------------------------------------------------------------------------------
auto endpoint::_ensure_incoming(const message_id msg_id) noexcept
  -> incoming_state& {
    auto incoming{find(_incoming, msg_id)};
    if(not incoming) {
        incoming.emplace(msg_id, default_selector);
    }
    assert(incoming and *incoming);
    return **incoming;
}
//------------------------------------------------------------------------------
auto endpoint::_find_incoming(const message_id msg_id) const noexcept
  -> optional_reference<incoming_state> {
    if(const auto incoming{find(_incoming, msg_id)}) {
        assert(*incoming);
        return incoming->ref();
    }
    return {};
}
//------------------------------------------------------------------------------
auto endpoint::_get_incoming(const message_id msg_id) const noexcept
  -> incoming_state& {
    auto incoming{find(_incoming, msg_id)};
    assert(incoming and *incoming);
    return **incoming;
}
//------------------------------------------------------------------------------
endpoint::endpoint(main_ctx_object obj) noexcept
  : main_ctx_object{std::move(obj)} {
    _declare_states();
}
//------------------------------------------------------------------------------
endpoint::endpoint(const identifier id, main_ctx_parent parent) noexcept
  : main_ctx_object{id, parent} {
    _declare_states();
}
//------------------------------------------------------------------------------
auto endpoint::_uptime_seconds() noexcept -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::steady_clock::now() - _startup_time)
      .count();
}
//------------------------------------------------------------------------------
auto endpoint::_process_blobs() noexcept -> work_done {
    some_true something_done;
    const auto post_handler{make_callable_ref<&endpoint::post>(this)};

    something_done(_blobs.handle_complete() > 0);
    const auto opt_max_size = max_data_size();
    if(opt_max_size) [[likely]] {
        something_done(_blobs.update(post_handler, *opt_max_size));
        something_done(_blobs.process_outgoing(post_handler, *opt_max_size, 2));
    }
    return something_done;
}
//------------------------------------------------------------------------------
auto endpoint::_do_send(const message_id msg_id, message_view message) noexcept
  -> bool {
    assert(has_id());
    message.set_source_id(_endpoint_id);
    if(_connection and _connection->send(msg_id, message)) [[likely]] {
        ++_stats.sent_messages;
        if(not _had_working_connection) [[unlikely]] {
            _had_working_connection = true;
            connection_established(has_id());
        }
        return true;
    }

    if(_had_working_connection) {
        _had_working_connection = false;
        connection_lost();
    }
    return false;
}
//------------------------------------------------------------------------------
auto endpoint::_handle_send(
  const message_id msg_id,
  const message_age,
  const message_view& message) noexcept -> bool {
    // TODO: use message age
    return _do_send(msg_id, message);
}
//------------------------------------------------------------------------------
auto endpoint::_handle_assign_id(const message_view& message) noexcept
  -> message_handling_result {
    if(not has_id()) {
        _router_id = message.source_id;
        _endpoint_id = message.target_id;
        id_assigned(_endpoint_id);
        log_info("assigned endpoint id ${eptId} by router ${routerId}")
          .tag("asignEptId")
          .arg("routerId", _router_id)
          .arg("eptId", get_id());
    }
    return was_handled;
}
//------------------------------------------------------------------------------
auto endpoint::_handle_confirm_id(const message_view& message) noexcept
  -> message_handling_result {
    if(not has_id()) {
        _router_id = message.source_id;
        _endpoint_id = message.target_id;
        if(get_id() == get_preconfigured_id()) [[likely]] {
            id_assigned(_endpoint_id);
            log_info("confirmed endpoint id ${eptId} by router ${routerId}")
              .tag("cnfrmEptId")
              .arg("routerId", _router_id)
              .arg("eptId", get_id());
            // send request for router certificate
            post(msgbus_id{"rtrCertQry"}, {});
        } else {
            log_error("mismatching preconfigured and confirmed ids")
              .arg("confirmed", get_id())
              .arg("preconfed", get_preconfigured_id());
        }
    }
    return was_handled;
}
//------------------------------------------------------------------------------
auto endpoint::_handle_password_request(const message_view& message) noexcept
  -> message_handling_result {
    if(is_valid_id(_router_id) and (message.source_id == _router_id)) {
        if(has_id() and (message.target_id == _endpoint_id)) {
            memory::buffer encrypted;
            if(main_context().encrypt_shared_password(
                 message.data(), "msgbus.router.password", encrypted)) {
                message_view response{view(encrypted)};
                response.setup_response(message);
                _do_send(msgbus_id{"encRutrPwd"}, response);
            }
        }
    }
    return was_handled;
}
//------------------------------------------------------------------------------
auto endpoint::_handle_blob_fragment(const message_view& message) noexcept
  -> message_handling_result {
    if(_blobs.process_incoming(message)) {
        _blobs.fetch_all(_store_handler);
    }
    return was_handled;
}
//------------------------------------------------------------------------------
auto endpoint::_handle_blob_resend(const message_view& message) noexcept
  -> message_handling_result {
    _blobs.process_resend(message);
    return was_handled;
}
//------------------------------------------------------------------------------
auto endpoint::_handle_flow_info(const message_view& message) noexcept
  -> message_handling_result {
    message_flow_info flow_info{};
    if(default_deserialize(flow_info, message.content())) [[likely]] {
        if(_flow_info != flow_info) {
            if(
              (_flow_info.average_message_age() < _flow_age_warning) and
              (flow_info.average_message_age() >= _flow_age_warning)) {
                log_warning("average message age is too high:  ${avgMsgAge}")
                  .tag("msgAgeHigh")
                  .arg("warnLimit", _flow_age_warning)
                  .arg("avgMsgAge", flow_info.average_message_age());
            } else if(
              (flow_info.average_message_age() < _flow_age_warning) and
              (_flow_info.average_message_age() >= _flow_age_warning)) {
                log_change(
                  "average message age returned to normal: ${avgMsgAge}")
                  .tag("msgAgeNorm")
                  .arg("warnLimit", _flow_age_warning)
                  .arg("avgMsgAge", flow_info.average_message_age());
            }
            _flow_info = flow_info;
            log_debug("changes in message flow information")
              .tag("msgFlowInf")
              .arg("avgMsgAge", flow_average_message_age());
        }
    }
    return was_handled;
}
//------------------------------------------------------------------------------
auto endpoint::_handle_certificate_query(const message_view& message) noexcept
  -> message_handling_result {
    post_certificate(message.source_id, message.sequence_no);
    return was_handled;
}
//------------------------------------------------------------------------------
auto endpoint::_handle_endpoint_certificate(const message_view& message) noexcept
  -> message_handling_result {
    log_trace("received remote endpoint certificate")
      .arg("source", message.source_id)
      .arg("pem", message.content());

    if(_context->add_remote_certificate_pem(
         message.source_id, message.content())) {
        log_debug("verified and stored remote endpoint certificate")
          .arg("endpoint", _endpoint_id)
          .arg("source", message.source_id);

        if(const auto nonce{_context->get_remote_nonce(message.source_id)}) {
            post_blob(
              msgbus_id{"eptSigNnce"},
              message.source_id,
              message.sequence_no,
              nonce,
              std::chrono::seconds(30),
              message_priority::normal);
            log_debug("sending nonce sign request")
              .arg("endpoint", _endpoint_id)
              .arg("target", message.source_id);
        }
    }
    return was_handled;
}
//------------------------------------------------------------------------------
auto endpoint::_handle_router_certificate(const message_view& message) noexcept
  -> message_handling_result {
    log_trace("received router certificate").arg("pem", message.content());

    if(_context->add_router_certificate_pem(message.content())) {
        log_debug("verified and stored router certificate");
    }
    return was_handled;
}
//------------------------------------------------------------------------------
auto endpoint::_handle_sign_nonce_request(const message_view& message) noexcept
  -> message_handling_result {
    if(const auto signature{_context->get_own_signature(message.content())}) {
        post_blob(
          msgbus_id{"eptNnceSig"},
          message.source_id,
          message.sequence_no,
          signature,
          std::chrono::seconds(30),
          message_priority::normal);
        log_debug("sending nonce signature")
          .arg("endpoint", _endpoint_id)
          .arg("target", message.source_id);
    }
    return was_handled;
}
//------------------------------------------------------------------------------
auto endpoint::_handle_signed_nonce(const message_view& message) noexcept
  -> message_handling_result {
    if(_context->verify_remote_signature(
         message.content(), message.source_id)) {
        log_debug("verified nonce signature")
          .arg("endpoint", _endpoint_id)
          .arg("source", message.source_id);
    }
    return was_handled;
}
//------------------------------------------------------------------------------
auto endpoint::_handle_topology_query(const message_view& message) noexcept
  -> message_handling_result {
    endpoint_topology_info info{};
    info.endpoint_id = _endpoint_id;
    info.instance_id = _instance_id;
    auto temp{default_serialize_buffer_for(info)};
    if(const auto serialized{default_serialize(info, cover(temp))}) [[likely]] {
        message_view response{*serialized};
        response.setup_response(message);
        if(post(msgbus_id{"topoEndpt"}, response)) [[likely]] {
            return was_handled;
        }
    }
    log_warning("failed to respond to topology query from ${source}")
      .arg("bufSize", temp.size())
      .arg("source", message.source_id);
    return was_not_handled;
}
//------------------------------------------------------------------------------
auto endpoint::_handle_stats_query(const message_view& message) noexcept
  -> message_handling_result {
    _stats.sent_messages = _stats.sent_messages;
    _stats.uptime_seconds = _uptime_seconds();

    auto temp{default_serialize_buffer_for(_stats)};
    if(const auto serialized{default_serialize(_stats, cover(temp))})
      [[likely]] {
        message_view response{*serialized};
        response.setup_response(message);
        if(post(msgbus_id{"statsEndpt"}, response)) [[likely]] {
            return was_handled;
        }
    }
    log_warning("failed to respond to statistics query from ${source}")
      .arg("bufSize", temp.size())
      .arg("source", message.source_id);
    return was_not_handled;
}
//------------------------------------------------------------------------------
auto endpoint::_handle_special(
  const message_id msg_id,
  const message_view& message) noexcept -> message_handling_result {

    assert(_context);
    if(is_special_message(msg_id)) {
        log_debug("endpoint handling special message ${message}")
          .arg("message", msg_id)
          .arg("endpoint", _endpoint_id)
          .arg("target", message.target_id)
          .arg("source", message.source_id);

        switch(msg_id.method_id()) {
            case id_v("blobFrgmnt"):
                return _handle_blob_fragment(message);
            case id_v("blobResend"):
                return _handle_blob_resend(message);
            case id_v("assignId"):
                return _handle_assign_id(message);
            case id_v("confirmId"):
                return _handle_confirm_id(message);
            case id_v("msgFlowInf"):
                return _handle_flow_info(message);
            case id_v("eptCertQry"):
                return _handle_certificate_query(message);
            case id_v("eptCertPem"):
                return _handle_endpoint_certificate(message);
            case id_v("eptSigNnce"):
                return _handle_sign_nonce_request(message);
            case id_v("eptNnceSig"):
                return _handle_signed_nonce(message);
            case id_v("rtrCertPem"):
                return _handle_router_certificate(message);
            case id_v("topoQuery"):
                return _handle_topology_query(message);
            case id_v("statsQuery"):
                return _handle_stats_query(message);
            case id_v("reqRutrPwd"):
                return _handle_password_request(message);
            case id_v("ping"):
            case id_v("pong"):
            case id_v("subscribTo"):
            case id_v("unsubFrom"):
            case id_v("notSubTo"):
            case id_v("qrySubscrp"):
            case id_v("qrySubscrb"):
            case id_v("byeByeEndp"):
            case id_v("byeByeRutr"):
            case id_v("byeByeBrdg"):
            case id_v("stillAlive"):
            case id_v("topoRutrCn"):
            case id_v("topoBrdgCn"):
            case id_v("topoEndpt"):
                return should_be_stored;
        }

        if(has_id() and (message.source_id == _endpoint_id)) [[unlikely]] {
            ++_stats.dropped_messages;
            log_warning("received own special message ${message}")
              .arg("message", msg_id);
            return was_handled;
        }
        log_warning("unhandled special message ${message} from ${source}")
          .arg("message", msg_id)
          .arg("source", message.source_id)
          .arg("data", message.data());
    }
    return should_be_stored;
}
//------------------------------------------------------------------------------
auto endpoint::_store_message(
  const message_id msg_id,
  const message_age msg_age,
  const message_view& message) noexcept -> bool {
    ++_stats.received_messages;
    if(_handle_special(msg_id, message) == should_be_stored) {
        if(
          (message.target_id == _endpoint_id) or
          not is_valid_id(message.target_id)) [[likely]] {
            if(auto found{_find_incoming(msg_id)}) [[likely]] {
                log_trace("stored message ${message}").arg("message", msg_id);
                found->queue.push(message).add_age(msg_age);
            } else {
                auto& state = _ensure_incoming(msg_id);
                assert(state.subscription_count == 0);
                log_debug("storing new type of message ${message}")
                  .arg("message", msg_id);
                state.queue.push(message).add_age(msg_age);
            }
        } else {
            ++_stats.dropped_messages;
            log_warning("trying to store message for target ${target}")
              .arg("self", _endpoint_id)
              .arg("target", message.target_id)
              .arg("message", msg_id);
            say_not_a_router();
        }
    }
    return true;
}
//------------------------------------------------------------------------------
auto endpoint::_accept_message(
  const message_id msg_id,
  const message_view& message) noexcept -> bool {
    if(_handle_special(msg_id, message) == was_handled) {
        return true;
    }
    if(auto found{_find_incoming(msg_id)}) [[likely]] {
        if(
          (message.target_id == _endpoint_id) or
          not is_valid_id(message.target_id)) {
            log_trace("accepted message ${message}").arg("message", msg_id);
            found->queue.push(message);
        }
        return true;
    }
    return false;
}
//------------------------------------------------------------------------------
void endpoint::add_certificate_pem(const memory::const_block blk) noexcept {
    assert(_context);
    if(_context->add_own_certificate_pem(blk)) {
        broadcast_certificate();
    }
}
//------------------------------------------------------------------------------
void endpoint::add_ca_certificate_pem(const memory::const_block blk) noexcept {
    assert(_context);
    if(_context->add_ca_certificate_pem(blk)) {
        broadcast_certificate();
    }
}
//------------------------------------------------------------------------------
auto endpoint::add_connection(unique_holder<connection> conn) noexcept -> bool {
    if(conn) [[likely]] {
        if(_connection) {
            log_debug("replacing connection type ${oldType} with ${newType}")
              .arg("oldType", _connection->type_id())
              .arg("newType", conn->type_id());
        } else {
            log_debug("adding connection type ${type}")
              .arg("type", conn->type_id());
        }
        _connection = std::move(conn);
        _log_no_connection.reset();
        return true;
    } else {
        log_error("assigning invalid connection");
    }
    return false;
}
//------------------------------------------------------------------------------
auto endpoint::is_usable() const noexcept -> bool {
    return _connection and _connection->is_usable();
}
//------------------------------------------------------------------------------
auto endpoint::max_data_size() const noexcept
  -> valid_if_positive<span_size_t> {
    span_size_t result{0};
    if(is_usable()) [[likely]] {
        if(const auto opt_max_size{_connection->max_data_size()}) {
            const auto max_size{*opt_max_size};
            if(result > 0) {
                if(result > max_size) {
                    result = max_size;
                }
            } else {
                result = max_size;
            }
        }
    }
    return {result};
}
//------------------------------------------------------------------------------
void endpoint::flush_outbox() noexcept {
    if(has_id()) [[likely]] {
        log_debug("flushing outbox (size: ${count})")
          .arg("count", _outgoing.count());
        _outgoing.fetch_all(make_callable_ref<&endpoint::_handle_send>(this));

        if(_connection) [[likely]] {
            _connection->update();
            _connection->cleanup();
        }
    }
}
//------------------------------------------------------------------------------
auto endpoint::set_next_sequence_id(
  const message_id msg_id,
  message_info& message) noexcept -> bool {
    assert(_context);
    message.set_sequence_no(_context->next_sequence_no(msg_id));
    return true;
}
//------------------------------------------------------------------------------
auto endpoint::post_signed(
  const message_id msg_id,
  const message_view msg_view) noexcept -> bool {
    if(const auto opt_size = max_data_size()) {
        const auto max_size = *opt_size;
        return _outgoing.push_if(
          [this, msg_id, &msg_view, max_size](
            message_id& dst_msg_id,
            message_timestamp&,
            stored_message& message) {
              message.assign(msg_view);
              if(message.store_and_sign(
                   msg_view.content(), max_size, ctx(), *this)) {
                  dst_msg_id = msg_id;
                  return true;
              }
              return false;
          },
          max_size);
    }
    return false;
}
//------------------------------------------------------------------------------
auto endpoint::_update_no_connection() noexcept -> work_done {
    some_true something_done{};
    log_warning(_log_no_connection, "endpoint has no connection")
      .tag("noConnect");
    if(_had_working_connection) {
        _had_working_connection = false;
        connection_lost();
        something_done();
    }
    return something_done;
}
//------------------------------------------------------------------------------
auto endpoint::_update_request_id() noexcept -> work_done {
    some_true something_done{};
    if(not has_preconfigured_id()) {
        log_debug("requesting endpoint id");
        _connection->send(msgbus_id{"requestId"}, {});
        ++_stats.sent_messages;
        _no_id_timeout.reset();
        something_done();
    }
    return something_done;
}
//------------------------------------------------------------------------------
auto endpoint::_update_check_id() noexcept -> work_done {
    some_true something_done{};
    if(has_id()) {
        log_debug("announcing endpoint id ${id} assigned by router")
          .arg("id", get_id());
        // send the endpoint id through all connections
        _do_send(msgbus_id{"annEndptId"}, {});
        // send request for router certificate
        _do_send(msgbus_id{"rtrCertQry"}, {});
        something_done();
    } else if(has_preconfigured_id()) {
        if(_no_id_timeout) {
            log_debug("announcing preconfigured endpoint id ${id}")
              .arg("id", get_preconfigured_id());
            // send the endpoint id through all connections
            message_view ann_in_msg{};
            ann_in_msg.set_source_id(get_preconfigured_id());
            _connection->send(msgbus_id{"annEndptId"}, ann_in_msg);
            ++_stats.sent_messages;
            _no_id_timeout.reset();
            something_done();
        }
    }
    return something_done;
}
//------------------------------------------------------------------------------
auto endpoint::_update_send_outbox() noexcept -> work_done {

    log_debug("sending ${count} messages from outbox")
      .arg("count", _outgoing.count());
    return _outgoing.fetch_all(
      make_callable_ref<&endpoint::_handle_send>(this));
}
//------------------------------------------------------------------------------
auto endpoint::update() noexcept -> work_done {
    static const auto exec_time_id{register_time_interval("busUpdate")};
    const auto exec_time{measure_time_interval(exec_time_id)};
    some_true something_done{};

    something_done(_process_blobs());

    if(not _connection) [[unlikely]] {
        something_done(_update_no_connection());
    }

    const bool had_id{has_id()};
    if(_connection) [[likely]] {
        if(_had_working_connection) [[likely]] {
            if(not _connection->is_usable()) [[unlikely]] {
                _had_working_connection = false;
                connection_lost();
                something_done();
            }
        } else if(_connection->is_usable()) {
            _had_working_connection = true;
            connection_established(had_id);
            something_done();
        }
        if(not had_id and _no_id_timeout) [[unlikely]] {
            something_done(_update_request_id());
        }
        something_done(_connection->update());
        something_done(_connection->fetch_messages(_store_handler));

        // if processing the messages assigned the endpoint id
        if(not had_id) [[unlikely]] {
            something_done(_update_check_id());
        }
    }

    if(_should_notify_alive) [[unlikely]] {
        say_still_alive();
    }

    // if we have a valid id and we have messages in outbox
    if(has_id() and not _outgoing.empty()) [[unlikely]] {
        something_done(_update_send_outbox());
    }

    return something_done;
}
//------------------------------------------------------------------------------
void endpoint::subscribe(const message_id msg_id) noexcept {
    auto& state = _ensure_incoming(msg_id);
    if(not state.subscription_count) {
        log_debug("subscribing to message ${message}").arg("message", msg_id);
    }
    ++state.subscription_count;
}
//------------------------------------------------------------------------------
void endpoint::unsubscribe(const message_id msg_id) noexcept {
    if(const auto found{find(_incoming, msg_id)}) {
        assert(*found);
        auto& state = **found;
        if(--state.subscription_count <= 0) {
            _incoming.erase(found.position());
            log_debug("unsubscribing from message ${message}")
              .arg("message", msg_id);
        }
    }
}
//------------------------------------------------------------------------------
auto endpoint::say_not_a_router() noexcept -> bool {
    log_debug("saying not a router");
    return post(msgbus_id{"notARouter"}, {});
}
//------------------------------------------------------------------------------
auto endpoint::say_still_alive() noexcept -> bool {
    log_trace("saying still alive");
    message_view msg{};
    msg.set_sequence_no(_instance_id);
    return post(msgbus_id{"stillAlive"}, msg);
}
//------------------------------------------------------------------------------
auto endpoint::say_bye() noexcept -> bool {
    log_debug("saying bye-bye");
    return post(msgbus_id{"byeByeEndp"}, {});
}
//------------------------------------------------------------------------------
void endpoint::post_meta_message(
  const message_id meta_msg_id,
  const message_id msg_id) noexcept {
    auto temp{default_serialize_buffer_for(msg_id)};
    if(const auto serialized{
         default_serialize_message_type(msg_id, cover(temp))}) [[likely]] {
        message_view meta_msg{*serialized};
        meta_msg.set_sequence_no(_instance_id);
        post(meta_msg_id, meta_msg);
    } else {
        log_debug("failed to serialize meta-message ${meta}")
          .arg("meta", meta_msg_id)
          .arg("message", msg_id);
    }
}
//------------------------------------------------------------------------------
void endpoint::post_meta_message_to(
  const endpoint_id_t target_id,
  const message_id meta_msg_id,
  const message_id msg_id) noexcept {
    auto temp{default_serialize_buffer_for(msg_id)};
    if(const auto serialized{
         default_serialize_message_type(msg_id, cover(temp))}) [[likely]] {
        message_view meta_msg{*serialized};
        meta_msg.set_target_id(target_id);
        meta_msg.set_sequence_no(_instance_id);
        post(meta_msg_id, meta_msg);
    } else {
        log_debug("failed to serialize meta-message ${meta}")
          .arg("meta", meta_msg_id)
          .arg("target", target_id)
          .arg("message", msg_id);
    }
}
//------------------------------------------------------------------------------
void endpoint::say_subscribes_to(const message_id msg_id) noexcept {
    log_debug("announces subscription to message ${message}")
      .arg("message", msg_id);
    post_meta_message(msgbus_id{"subscribTo"}, msg_id);
}
//------------------------------------------------------------------------------
void endpoint::say_subscribes_to(
  const endpoint_id_t target_id,
  const message_id msg_id) noexcept {
    log_debug("announces subscription to message ${message}")
      .arg("target", target_id)
      .arg("message", msg_id);
    post_meta_message_to(target_id, msgbus_id{"subscribTo"}, msg_id);
}
//------------------------------------------------------------------------------
void endpoint::say_not_subscribed_to(
  const endpoint_id_t target_id,
  const message_id msg_id) noexcept {
    log_debug("denies subscription to message ${message}")
      .arg("target", target_id)
      .arg("message", msg_id);
    post_meta_message_to(target_id, msgbus_id{"notSubTo"}, msg_id);
}
//------------------------------------------------------------------------------
void endpoint::say_unsubscribes_from(const message_id msg_id) noexcept {
    log_debug("retracting subscription to message ${message}")
      .arg("message", msg_id);
    post_meta_message(msgbus_id{"unsubFrom"}, msg_id);
}
//------------------------------------------------------------------------------
void endpoint::query_subscriptions_of(const endpoint_id_t target_id) noexcept {
    log_debug("querying subscribed messages of endpoint ${target}")
      .arg("target", target_id);
    message_view msg{};
    msg.set_target_id(target_id);
    post(msgbus_id{"qrySubscrp"}, msg);
}
//------------------------------------------------------------------------------
void endpoint::query_subscribers_of(const message_id msg_id) noexcept {
    log_debug("querying subscribers of message ${message}")
      .arg("message", msg_id);
    post_meta_message(msgbus_id{"qrySubscrb"}, msg_id);
}
//------------------------------------------------------------------------------
void endpoint::clear_block_list() noexcept {
    log_debug("sending clear block list");
    post(msgbus_id{"clrBlkList"}, {});
}
//------------------------------------------------------------------------------
void endpoint::block_message_type(const message_id msg_id) noexcept {
    log_debug("blocking message ${message}").arg("message", msg_id);
    post_meta_message(msgbus_id{"msgBlkList"}, msg_id);
}
//------------------------------------------------------------------------------
void endpoint::clear_allow_list() noexcept {
    log_debug("sending clear allow list");
    post(msgbus_id{"clrAlwList"}, {});
}
//------------------------------------------------------------------------------
void endpoint::allow_message_type(const message_id msg_id) noexcept {
    log_debug("allowing message ${message}").arg("message", msg_id);
    post_meta_message(msgbus_id{"msgAlwList"}, msg_id);
}
//------------------------------------------------------------------------------
auto endpoint::post_certificate(
  const endpoint_id_t target_id,
  const blob_id_t target_blob_id) noexcept -> bool {
    assert(_context);
    if(const auto cert_pem{_context->get_own_certificate_pem()}) {
        return post_blob(
          msgbus_id{"eptCertPem"},
          target_id,
          target_blob_id,
          cert_pem,
          adjusted_duration(std::chrono::seconds{30}),
          message_priority::normal);
    }
    log_debug("no endpoint certificate to send yet");
    return false;
}
//------------------------------------------------------------------------------
auto endpoint::broadcast_certificate() noexcept -> bool {
    assert(_context);
    if(const auto cert_pem{_context->get_own_certificate_pem()}) {
        return broadcast_blob(
          msgbus_id{"eptCertPem"},
          cert_pem,
          adjusted_duration(std::chrono::seconds{30}),
          message_priority::normal);
    }
    log_debug("no endpoint certificate to broadcast yet");
    return false;
}
//------------------------------------------------------------------------------
void endpoint::query_certificate_of(const endpoint_id_t endpoint_id) noexcept {
    log_debug("querying certificate of endpoint ${endpoint}")
      .arg("endpoint", endpoint_id);
    message_view msg{};
    msg.set_target_id(endpoint_id);
    post(msgbus_id{"eptCertQry"}, msg);
}
//------------------------------------------------------------------------------
auto endpoint::process_one(
  const message_id msg_id,
  const method_handler handler) noexcept -> bool {
    if(const auto found{_find_incoming(msg_id)}) [[likely]] {
        const message_context msg_ctx{*this, msg_id};
        return found->queue.process_one(msg_ctx, handler);
    }
    return false;
}
//------------------------------------------------------------------------------
auto endpoint::process_all(
  const message_id msg_id,
  const method_handler handler) noexcept -> span_size_t {
    if(const auto found{_find_incoming(msg_id)}) [[likely]] {
        const message_context msg_ctx{*this, msg_id};
        return found->queue.process_all(msg_ctx, handler);
    }
    return 0;
}
//------------------------------------------------------------------------------
auto endpoint::process_everything(const method_handler handler) noexcept
  -> span_size_t {
    span_size_t result = 0;

    for(auto& [msg_id, state] : _incoming) {
        const message_context msg_ctx{*this, msg_id};
        result += state->queue.process_all(msg_ctx, handler);
    }
    return result;
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
#include <eagine/bool_aggregate.hpp>
#include <eagine/msgbus/context.hpp>

namespace eagine::msgbus {
//------------------------------------------------------------------------------
// endpoint
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::_uptime_seconds() noexcept -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::steady_clock::now() - _startup_time)
      .count();
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::_process_blobs() noexcept -> work_done {
    some_true something_done;
    something_done(_blobs.update(EAGINE_THIS_MEM_FUNC_REF(post)));
    const auto opt_max_size = max_data_size();
    if(opt_max_size) [[likely]] {
        something_done(_blobs.process_outgoing(
          EAGINE_THIS_MEM_FUNC_REF(post), extract(opt_max_size)));
    }
    return something_done;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::_do_send(const message_id msg_id, message_view message) noexcept
  -> bool {
    EAGINE_ASSERT(has_id());
    message.set_source_id(_endpoint_id);
    if(_connection && _connection->send(msg_id, message)) [[likely]] {
        ++_stats.sent_messages;
        if(!_had_working_connection) [[unlikely]] {
            _had_working_connection = true;
            connection_established(has_id());
        }
        log_trace("sending message ${message}")
          .arg("message", msg_id)
          .arg("target", message.target_id)
          .arg("source", message.source_id);
        return true;
    } else {
        if(_had_working_connection) {
            _had_working_connection = false;
            connection_lost();
        }
    }
    return false;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::_handle_assign_id(const message_view& message) noexcept
  -> message_handling_result {
    if(!has_id()) {
        _endpoint_id = message.target_id;
        id_assigned(_endpoint_id);
        log_debug("assigned endpoint id ${id} by router").arg("id", get_id());
    }
    return was_handled;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::_handle_confirm_id(const message_view& message) noexcept
  -> message_handling_result {
    if(!has_id()) {
        _endpoint_id = message.target_id;
        if(get_id() == get_preconfigured_id()) [[likely]] {
            id_assigned(_endpoint_id);
            log_debug("confirmed endpoint id ${id} by router")
              .arg("id", get_id());
            // send request for router certificate
            post(EAGINE_MSGBUS_ID(rtrCertQry), {});
        } else {
            log_error("mismatching preconfigured and confirmed ids")
              .arg("confirmed", get_id())
              .arg("preconfed", get_preconfigured_id());
        }
    }
    return was_handled;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::_handle_blob_fragment(const message_view& message) noexcept
  -> message_handling_result {
    if(_blobs.process_incoming(message)) {
        _blobs.fetch_all(_store_handler);
    }
    return was_handled;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::_handle_blob_resend(const message_view& message) noexcept
  -> message_handling_result {
    _blobs.process_resend(message);
    return was_handled;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::_handle_flow_info(const message_view& message) noexcept
  -> message_handling_result {
    default_deserialize(_flow_info, message.content());
    log_debug("changes in message flow information")
      .arg("avgMsgAge", flow_average_message_age());
    return was_handled;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::_handle_certificate_query(const message_view& message) noexcept
  -> message_handling_result {
    post_certificate(message.source_id, message.sequence_no);
    return was_handled;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
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
              EAGINE_MSGBUS_ID(eptSigNnce),
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
EAGINE_LIB_FUNC
auto endpoint::_handle_router_certificate(const message_view& message) noexcept
  -> message_handling_result {
    log_trace("received router certificate").arg("pem", message.content());

    if(_context->add_router_certificate_pem(message.content())) {
        log_debug("verified and stored router certificate");
    }
    return was_handled;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::_handle_sign_nonce_request(const message_view& message) noexcept
  -> message_handling_result {
    if(const auto signature{_context->get_own_signature(message.content())}) {
        post_blob(
          EAGINE_MSGBUS_ID(eptNnceSig),
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
EAGINE_LIB_FUNC
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
EAGINE_LIB_FUNC
auto endpoint::_handle_topology_query(const message_view& message) noexcept
  -> message_handling_result {
    endpoint_topology_info info{};
    info.endpoint_id = _endpoint_id;
    info.instance_id = _instance_id;
    auto temp{default_serialize_buffer_for(info)};
    if(const auto serialized{default_serialize(info, cover(temp))}) {
        message_view response{extract(serialized)};
        response.setup_response(message);
        if(post(EAGINE_MSGBUS_ID(topoEndpt), response)) {
            return was_handled;
        }
    }
    log_warning("failed to respond to topology query from ${source}")
      .arg("bufSize", temp.size())
      .arg("source", message.source_id);
    return was_not_handled;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::_handle_stats_query(const message_view& message) noexcept
  -> message_handling_result {
    _stats.sent_messages = _stats.sent_messages;
    _stats.uptime_seconds = _uptime_seconds();

    auto temp{default_serialize_buffer_for(_stats)};
    if(const auto serialized{default_serialize(_stats, cover(temp))}) {
        message_view response{extract(serialized)};
        response.setup_response(message);
        if(post(EAGINE_MSGBUS_ID(statsEndpt), response)) {
            return was_handled;
        }
    }
    log_warning("failed to respond to statistics query from ${source}")
      .arg("bufSize", temp.size())
      .arg("source", message.source_id);
    return was_not_handled;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::_handle_special(
  const message_id msg_id,
  const message_view& message) noexcept -> message_handling_result {

    EAGINE_ASSERT(_context);
    if(is_special_message(msg_id)) [[unlikely]] {
        log_debug("endpoint handling special message ${message}")
          .arg("message", msg_id)
          .arg("endpoint", _endpoint_id)
          .arg("target", message.target_id)
          .arg("source", message.source_id);

        if(has_id() && (message.source_id == _endpoint_id)) [[unlikely]] {
            ++_stats.dropped_messages;
            log_warning("received own special message ${message}")
              .arg("message", msg_id);
            return was_handled;
        } else if(msg_id.has_method("blobFrgmnt")) {
            return _handle_blob_fragment(message);
        } else if(msg_id.has_method("blobResend")) {
            return _handle_blob_resend(message);
        } else if(msg_id.has_method("assignId")) {
            return _handle_assign_id(message);
        } else if(msg_id.has_method("confirmId")) {
            return _handle_confirm_id(message);
        } else if(
          msg_id.has_method("ping") || msg_id.has_method("pong") ||
          msg_id.has_method("subscribTo") || msg_id.has_method("unsubFrom") ||
          msg_id.has_method("notSubTo") || msg_id.has_method("qrySubscrp") ||
          msg_id.has_method("qrySubscrb")) {
            return should_be_stored;
        } else if(msg_id.has_method("msgFlowInf")) {
            return _handle_flow_info(message);
        } else if(msg_id.has_method("eptCertQry")) {
            return _handle_certificate_query(message);
        } else if(msg_id.has_method("eptCertPem")) {
            return _handle_endpoint_certificate(message);
        } else if(msg_id.has_method("eptSigNnce")) {
            return _handle_sign_nonce_request(message);
        } else if(msg_id.has_method("eptNnceSig")) {
            return _handle_signed_nonce(message);
        } else if(msg_id.has_method("rtrCertPem")) {
            return _handle_router_certificate(message);
        } else if(
          msg_id.has_method("byeByeEndp") || msg_id.has_method("byeByeRutr") ||
          msg_id.has_method("byeByeBrdg") || msg_id.has_method("stillAlive") ||
          msg_id.has_method("topoRutrCn") || msg_id.has_method("topoBrdgCn") ||
          msg_id.has_method("topoEndpt")) {
            return should_be_stored;
        } else if(msg_id.has_method("topoQuery")) {
            return _handle_topology_query(message);
        } else if(msg_id.has_method("statsQuery")) {
            return _handle_stats_query(message);
        }
        log_warning("unhandled special message ${message} from ${source}")
          .arg("message", msg_id)
          .arg("source", message.source_id)
          .arg("data", message.data());
    }
    return should_be_stored;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::_store_message(
  const message_id msg_id,
  const message_age msg_age,
  const message_view& message) noexcept -> bool {
    ++_stats.received_messages;
    if(_handle_special(msg_id, message) == should_be_stored) {
        if((message.target_id == _endpoint_id) || !is_valid_id(message.target_id)) {
            if(auto found{_find_incoming(msg_id)}) {
                log_trace("stored message ${message}").arg("message", msg_id);
                extract(found).queue.push(message).add_age(msg_age);
            } else {
                auto& state = _ensure_incoming(msg_id);
                EAGINE_ASSERT(state.subscription_count == 0);
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
EAGINE_LIB_FUNC
auto endpoint::_accept_message(
  const message_id msg_id,
  const message_view& message) noexcept -> bool {
    if(_handle_special(msg_id, message) == was_handled) {
        return true;
    }
    if(auto found{_find_incoming(msg_id)}) {
        if((message.target_id == _endpoint_id) || !is_valid_id(message.target_id)) {
            log_trace("accepted message ${message}").arg("message", msg_id);
            extract(found).queue.push(message);
        }
        return true;
    }
    return false;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
void endpoint::add_certificate_pem(const memory::const_block blk) noexcept {
    EAGINE_ASSERT(_context);
    if(_context) {
        if(_context->add_own_certificate_pem(blk)) {
            broadcast_certificate();
        }
    }
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
void endpoint::add_ca_certificate_pem(const memory::const_block blk) noexcept {
    EAGINE_ASSERT(_context);
    if(_context) {
        if(_context->add_ca_certificate_pem(blk)) {
            broadcast_certificate();
        }
    }
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::add_connection(std::unique_ptr<connection> conn) noexcept
  -> bool {
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
        return true;
    } else {
        log_error("assigning invalid connection");
    }
    return false;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::is_usable() const noexcept -> bool {
    if(_connection) [[likely]] {
        if(_connection->is_usable()) {
            return true;
        }
    }
    return false;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::max_data_size() const noexcept
  -> valid_if_positive<span_size_t> {
    span_size_t result{0};
    if(_connection) [[likely]] {
        if(_connection->is_usable()) {
            if(const auto opt_max_size = _connection->max_data_size()) {
                const auto max_size = extract(opt_max_size);
                if(result > 0) {
                    if(result > max_size) {
                        result = max_size;
                    }
                } else {
                    result = max_size;
                }
            }
        }
    }
    return {result};
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
void endpoint::flush_outbox() noexcept {
    if(has_id()) {
        log_debug("flushing outbox (size: ${count})")
          .arg("count", _outgoing.count());

        _outgoing.fetch_all(EAGINE_THIS_MEM_FUNC_REF(_handle_send));

        if(_connection) [[likely]] {
            _connection->update();
            _connection->cleanup();
        }
    }
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::set_next_sequence_id(
  const message_id msg_id,
  message_info& message) noexcept -> bool {
    EAGINE_ASSERT(_context);
    message.set_sequence_no(_context->next_sequence_no(msg_id));
    return true;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::post_signed(
  const message_id msg_id,
  const message_view msg_view) noexcept -> bool {
    if(const auto opt_size = max_data_size()) {
        const auto max_size = extract(opt_size);
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
EAGINE_LIB_FUNC
auto endpoint::update() noexcept -> work_done {
    some_true something_done{};

    something_done(_process_blobs());

    if(!_connection) [[unlikely]] {
        log_warning("endpoint has no connection");
    }

    const bool had_id = has_id();
    if(_connection) [[likely]] {
        if(!_had_working_connection) [[unlikely]] {
            _had_working_connection = true;
            connection_established(had_id);
        }
        if(!had_id && _no_id_timeout) [[unlikely]] {
            if(!has_preconfigured_id()) {
                log_debug("requesting endpoint id");
                _connection->send(EAGINE_MSGBUS_ID(requestId), {});
                ++_stats.sent_messages;
                _no_id_timeout.reset();
                something_done();
            }
        }
        something_done(_connection->update());
        something_done(_connection->fetch_messages(_store_handler));
    }

    // if processing the messages assigned the endpoint id
    if(!had_id) [[unlikely]] {
        if(_connection) {
            if(has_id()) {
                log_debug("announcing endpoint id ${id} assigned by router")
                  .arg("id", get_id());
                // send the endpoint id through all connections
                _do_send(EAGINE_MSGBUS_ID(annEndptId), {});
                // send request for router certificate
                _do_send(EAGINE_MSGBUS_ID(rtrCertQry), {});
                something_done();
            } else if(has_preconfigured_id()) {
                if(_no_id_timeout) {
                    log_debug("announcing preconfigured endpoint id ${id}")
                      .arg("id", get_preconfigured_id());
                    // send the endpoint id through all connections
                    message_view ann_in_msg{};
                    ann_in_msg.set_source_id(get_preconfigured_id());
                    _connection->send(EAGINE_MSGBUS_ID(annEndptId), ann_in_msg);
                    ++_stats.sent_messages;
                    _no_id_timeout.reset();
                    something_done();
                }
            }
        }
    }

    if(_should_notify_alive) {
        say_still_alive();
    }

    // if we have a valid id and we have messages in outbox
    if(has_id() && !_outgoing.empty()) [[unlikely]] {
        log_debug("sending ${count} messages from outbox")
          .arg("count", _outgoing.count());
        something_done(
          _outgoing.fetch_all(EAGINE_THIS_MEM_FUNC_REF(_handle_send)));
    }

    return something_done;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
void endpoint::subscribe(const message_id msg_id) noexcept {
    auto& state = _ensure_incoming(msg_id);
    if(!state.subscription_count) {
        log_debug("subscribing to message ${message}").arg("message", msg_id);
    }
    ++state.subscription_count;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
void endpoint::unsubscribe(const message_id msg_id) noexcept {
    auto pos = _incoming.find(msg_id);
    if(pos != _incoming.end()) {
        EAGINE_ASSERT(pos->second);
        auto& state = *pos->second;
        if(--state.subscription_count <= 0) {
            _incoming.erase(pos);
            log_debug("unsubscribing from message ${message}")
              .arg("message", msg_id);
        }
    }
}
//------------------------------------------------------------------------------
auto endpoint::say_not_a_router() noexcept -> bool {
    log_debug("saying not a router");
    return post(EAGINE_MSGBUS_ID(notARouter), {});
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::say_still_alive() noexcept -> bool {
    log_trace("saying still alive");
    message_view msg{};
    msg.set_sequence_no(_instance_id);
    return post(EAGINE_MSGBUS_ID(stillAlive), msg);
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::say_bye() noexcept -> bool {
    log_debug("saying bye-bye");
    return post(EAGINE_MSGBUS_ID(byeByeEndp), {});
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
void endpoint::post_meta_message(
  const message_id meta_msg_id,
  const message_id msg_id) noexcept {
    auto temp{default_serialize_buffer_for(msg_id)};
    if(const auto serialized{
         default_serialize_message_type(msg_id, cover(temp))}) {
        message_view meta_msg{extract(serialized)};
        meta_msg.set_sequence_no(_instance_id);
        post(meta_msg_id, meta_msg);
    } else {
        log_debug("failed to serialize meta-message ${meta}")
          .arg("meta", meta_msg_id)
          .arg("message", msg_id);
    }
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
void endpoint::post_meta_message_to(
  const identifier_t target_id,
  const message_id meta_msg_id,
  const message_id msg_id) noexcept {
    auto temp{default_serialize_buffer_for(msg_id)};
    if(const auto serialized{
         default_serialize_message_type(msg_id, cover(temp))}) {
        message_view meta_msg{extract(serialized)};
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
EAGINE_LIB_FUNC
void endpoint::say_subscribes_to(const message_id msg_id) noexcept {
    log_debug("announces subscription to message ${message}")
      .arg("message", msg_id);
    post_meta_message(EAGINE_MSGBUS_ID(subscribTo), msg_id);
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
void endpoint::say_subscribes_to(
  const identifier_t target_id,
  const message_id msg_id) noexcept {
    log_debug("announces subscription to message ${message}")
      .arg("target", target_id)
      .arg("message", msg_id);
    post_meta_message_to(target_id, EAGINE_MSGBUS_ID(subscribTo), msg_id);
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
void endpoint::say_not_subscribed_to(
  const identifier_t target_id,
  const message_id msg_id) noexcept {
    log_debug("denies subscription to message ${message}")
      .arg("target", target_id)
      .arg("message", msg_id);
    post_meta_message_to(target_id, EAGINE_MSGBUS_ID(notSubTo), msg_id);
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
void endpoint::say_unsubscribes_from(const message_id msg_id) noexcept {
    log_debug("retracting subscription to message ${message}")
      .arg("message", msg_id);
    post_meta_message(EAGINE_MSGBUS_ID(unsubFrom), msg_id);
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
void endpoint::query_subscriptions_of(const identifier_t target_id) noexcept {
    log_debug("querying subscribed messages of endpoint ${target}")
      .arg("target", target_id);
    message_view msg{};
    msg.set_target_id(target_id);
    post(EAGINE_MSGBUS_ID(qrySubscrp), msg);
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
void endpoint::query_subscribers_of(const message_id msg_id) noexcept {
    log_debug("querying subscribers of message ${message}")
      .arg("message", msg_id);
    post_meta_message(EAGINE_MSGBUS_ID(qrySubscrb), msg_id);
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
void endpoint::clear_block_list() noexcept {
    log_debug("sending clear block list");
    post(EAGINE_MSGBUS_ID(clrBlkList), {});
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
void endpoint::block_message_type(const message_id msg_id) noexcept {
    log_debug("blocking message ${message}").arg("message", msg_id);
    post_meta_message(EAGINE_MSGBUS_ID(msgBlkList), msg_id);
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
void endpoint::clear_allow_list() noexcept {
    log_debug("sending clear allow list");
    post(EAGINE_MSGBUS_ID(clrAlwList), {});
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
void endpoint::allow_message_type(const message_id msg_id) noexcept {
    log_debug("allowing message ${message}").arg("message", msg_id);
    post_meta_message(EAGINE_MSGBUS_ID(msgAlwList), msg_id);
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::post_certificate(
  const identifier_t target_id,
  const blob_id_t target_blob_id) noexcept -> bool {
    EAGINE_ASSERT(_context);
    if(const auto cert_pem{_context->get_own_certificate_pem()}) {
        return post_blob(
          EAGINE_MSGBUS_ID(eptCertPem),
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
EAGINE_LIB_FUNC
auto endpoint::broadcast_certificate() noexcept -> bool {
    EAGINE_ASSERT(_context);
    if(const auto cert_pem{_context->get_own_certificate_pem()}) {
        return broadcast_blob(
          EAGINE_MSGBUS_ID(eptCertPem),
          cert_pem,
          adjusted_duration(std::chrono::seconds{30}),
          message_priority::normal);
    }
    log_debug("no endpoint certificate to broadcast yet");
    return false;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
void endpoint::query_certificate_of(const identifier_t endpoint_id) noexcept {
    log_debug("querying certificate of endpoint ${endpoint}")
      .arg("endpoint", endpoint_id);
    message_view msg{};
    msg.set_target_id(endpoint_id);
    post(EAGINE_MSGBUS_ID(eptCertQry), msg);
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::process_one(
  const message_id msg_id,
  const method_handler handler) noexcept -> bool {
    if(const auto found{_find_incoming(msg_id)}) {
        const message_context msg_ctx{*this, msg_id};
        return extract(found).queue.process_one(msg_ctx, handler);
    }
    return false;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::process_all(
  const message_id msg_id,
  const method_handler handler) noexcept -> span_size_t {
    if(const auto found{_find_incoming(msg_id)}) {
        const message_context msg_ctx{*this, msg_id};
        return extract(found).queue.process_all(msg_ctx, handler);
    }
    return 0;
}
//------------------------------------------------------------------------------
EAGINE_LIB_FUNC
auto endpoint::process_everything(const method_handler handler) noexcept
  -> span_size_t {
    span_size_t result = 0;

    for(auto& [msg_id, state] : _incoming) {
        const message_context msg_ctx{*this, msg_id};
        result += extract(state).queue.process_all(msg_ctx, handler);
    }
    return result;
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

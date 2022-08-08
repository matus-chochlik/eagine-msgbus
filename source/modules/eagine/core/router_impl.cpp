/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

module eagine.msgbus.core;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.container;
import eagine.core.utility;
import eagine.core.valid_if;
import eagine.core.runtime;
import eagine.core.main_ctx;
import <array>;
import <latch>;
import <thread>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
// routed_node
//------------------------------------------------------------------------------
static inline auto message_id_list_contains(
  const std::vector<message_id>& list,
  const message_id& entry) noexcept -> bool {
    return std::find(list.begin(), list.end(), entry) != list.end();
}
//------------------------------------------------------------------------------
static inline void message_id_list_add(
  std::vector<message_id>& list,
  const message_id& entry) noexcept {
    if(!message_id_list_contains(list, entry)) {
        list.push_back(entry);
    }
}
//------------------------------------------------------------------------------
static inline void message_id_list_remove(
  std::vector<message_id>& list,
  const message_id& entry) noexcept {
    const auto pos = std::find(list.begin(), list.end(), entry);
    if(pos != list.end()) {
        list.erase(pos);
    }
}
//------------------------------------------------------------------------------
routed_node::routed_node() noexcept {
    message_block_list.reserve(8);
    message_allow_list.reserve(8);
}
//------------------------------------------------------------------------------
auto routed_node::is_allowed(const message_id msg_id) const noexcept -> bool {
    if(is_special_message(msg_id)) {
        return true;
    }
    if(!message_allow_list.empty()) {
        return message_id_list_contains(message_allow_list, msg_id);
    }
    if(!message_block_list.empty()) {
        return !message_id_list_contains(message_block_list, msg_id);
    }
    return true;
}
//------------------------------------------------------------------------------
auto routed_node::send(
  const main_ctx_object& user,
  const message_id msg_id,
  const message_view& message) const noexcept -> bool {
    if(the_connection) [[likely]] {
        if(!the_connection->send(msg_id, message)) [[unlikely]] {
            user.log_debug("failed to send message to connected node");
            return false;
        }
    } else {
        user.log_debug("missing or unusable node connection");
        return false;
    }
    return true;
}
//------------------------------------------------------------------------------
void routed_node::block_message(const message_id msg_id) noexcept {
    message_id_list_add(message_block_list, msg_id);
}
//------------------------------------------------------------------------------
void routed_node::allow_message(const message_id msg_id) noexcept {
    message_id_list_add(message_allow_list, msg_id);
}
//------------------------------------------------------------------------------
// parent_router
//------------------------------------------------------------------------------
inline void parent_router::reset(
  std::unique_ptr<connection> a_connection) noexcept {
    the_connection = std::move(a_connection);
    confirmed_id = 0;
}
//------------------------------------------------------------------------------
inline auto parent_router::update(
  main_ctx_object& user,
  const identifier_t id_base) noexcept -> work_done {
    some_true something_done{};

    if(the_connection) [[likely]] {
        something_done(the_connection->update());
        if(the_connection->is_usable()) [[likely]] {
            if(!confirmed_id) [[unlikely]] {
                if(confirm_id_timeout) {
                    message_view announcement{};
                    announcement.set_source_id(id_base);
                    the_connection->send(msgbus_id{"announceId"}, announcement);
                    confirm_id_timeout.reset();
                    something_done();

                    user.log_debug("announcing id ${id} to parent router")
                      .arg("id", id_base);
                }
            }
            something_done(the_connection->update());
        } else {
            if(confirmed_id) {
                confirmed_id = 0;
                something_done();
                user.log_debug("lost connection to parent router");
            }
        }
    }
    return something_done;
}
//------------------------------------------------------------------------------
auto parent_router::send(
  const main_ctx_object& user,
  const message_id msg_id,
  const message_view& message) const noexcept -> bool {
    if(the_connection) [[likely]] {
        if(!the_connection->send(msg_id, message)) [[unlikely]] {
            user.log_debug("failed to send message to parent router");
            return false;
        }
    }
    return true;
}
//------------------------------------------------------------------------------
// router
//------------------------------------------------------------------------------
auto router::_uptime_seconds() noexcept -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::steady_clock::now() - _startup_time)
      .count();
}
//------------------------------------------------------------------------------
void router::add_certificate_pem(const memory::const_block blk) noexcept {
    if(_context) [[likely]] {
        _context->add_own_certificate_pem(blk);
    }
}
//------------------------------------------------------------------------------
void router::add_ca_certificate_pem(const memory::const_block blk) noexcept {
    if(_context) [[likely]] {
        _context->add_ca_certificate_pem(blk);
    }
}
//------------------------------------------------------------------------------
auto router::add_acceptor(std::shared_ptr<acceptor> an_acceptor) noexcept
  -> bool {
    if(an_acceptor) {
        log_info("adding connection acceptor")
          .tag("addAccptor")
          .arg("kind", an_acceptor->kind())
          .arg("type", an_acceptor->type_id());
        _acceptors.emplace_back(std::move(an_acceptor));
        return true;
    }
    return false;
}
//------------------------------------------------------------------------------
auto router::add_connection(std::unique_ptr<connection> a_connection) noexcept
  -> bool {
    if(a_connection) {
        log_info("assigning parent router connection")
          .tag("setCnnctin")
          .arg("kind", a_connection->kind())
          .arg("type", a_connection->type_id());
        _parent_router.reset(std::move(a_connection));
        return true;
    }
    return false;
}
//------------------------------------------------------------------------------
void router::_setup_from_config() {

    const auto id_count = extract_or(
      app_config().get<host_id_t>("msgbus.router.id_count"), 1U << 12U);

    const auto host_id =
      identifier_t(extract_or(main_context().system().host_id(), 0U));

    _id_base =
      extract_or(
        app_config().get<identifier_t>("msgbus.router.id_major"),
        host_id << 32U) +
      extract_or(app_config().get<identifier_t>("msgbus.router.id_minor"), 0U);

    if(_id_base) {
        _id_end = _id_base + id_count;
    } else {
        _id_base = 1;
        _id_end = id_count;
    }
    _id_sequence = _id_base + 1;

    log_info("using router id range ${base} - ${end} (${count})")
      .tag("idRange")
      .arg("count", id_count)
      .arg("base", _id_base)
      .arg("end", _id_end);
}
//------------------------------------------------------------------------------
auto router::_handle_accept() noexcept -> work_done {
    some_true something_done{};

    if(!_acceptors.empty()) [[likely]] {
        acceptor::accept_handler handler{
          this, member_function_constant_t<&router::_handle_connection>{}};
        for(auto& an_acceptor : _acceptors) {
            assert(an_acceptor);
            something_done(an_acceptor->update());
            something_done(an_acceptor->process_accepted(handler));
        }
    }
    return something_done;
}
//------------------------------------------------------------------------------
auto router::_handle_pending() noexcept -> work_done {
    some_true something_done{};

    if(!_pending.empty()) [[unlikely]] {
        identifier_t id = 0;
        bool maybe_router = true;
        auto handler =
          [&](message_id msg_id, message_age, const message_view& msg) {
              // this is a special message requesting endpoint id assignment
              if(msg_id == msgbus_id{"requestId"}) {
                  id = ~id;
                  return true;
              }
              // this is a special message containing endpoint id
              if(msg_id == msgbus_id{"annEndptId"}) {
                  id = msg.source_id;
                  maybe_router = false;
                  this->log_debug("received endpoint id ${id}").arg("id", id);
                  return true;
              }
              // this is a special message containing non-endpoint id
              if(msg_id == msgbus_id{"announceId"}) {
                  id = msg.source_id;
                  this->log_debug("received id ${id}").arg("id", id);
                  return true;
              }
              return false;
          };

        std::size_t idx = 0;
        while(idx < _pending.size()) {
            id = 0;
            auto& pending = _pending[idx];

            something_done(pending.the_connection->update());
            something_done(pending.the_connection->fetch_messages(
              {construct_from, handler}));
            something_done(pending.the_connection->update());
            // if we got the endpoint id message from the connection
            if(~id == 0) {
                _assign_id(pending.the_connection);
            } else if(id != 0) {
                log_info("adopting pending connection from ${cnterpart} ${id}")
                  .tag("adPendConn")
                  .arg("kind", pending.the_connection->kind())
                  .arg("type", pending.the_connection->type_id())
                  .arg("id", id)
                  .arg(
                    "cnterpart",
                    maybe_router ? string_view("non-endpoint")
                                 : string_view("endpoint"));

                // send the special message confirming assigned endpoint id
                message_view confirmation{};
                confirmation.set_source_id(_id_base).set_target_id(id);
                pending.the_connection->send(
                  msgbus_id{"confirmId"}, confirmation);

                auto pos = _nodes.find(id);
                if(pos == _nodes.end()) {
                    pos = _nodes.try_emplace(id).first;
                }
                pos->second.the_connection = std::move(pending.the_connection);
                pos->second.maybe_router = maybe_router;
                _pending.erase(_pending.begin() + signedness_cast(idx));
                _recently_disconnected.erase(id);
                something_done();
            } else {
                ++idx;
            }
        }
    }
    return something_done;
}
//------------------------------------------------------------------------------
auto router::_remove_timeouted() noexcept -> work_done {
    some_true something_done{};

    std::erase_if(_pending, [this, &something_done](auto& pending) {
        if(pending.age() > this->_pending_timeout) {
            something_done();
            log_warning("removing timeouted pending ${type} connection")
              .tag("rmPendConn")
              .arg("type", pending.the_connection->type_id())
              .arg("age", pending.age());
            return true;
        }
        return false;
    });

    _endpoint_infos.erase_if([this](auto& entry) {
        auto& [endpoint_id, info] = entry;
        if(info.is_outdated) {
            _endpoint_idx.erase(endpoint_id);
            _mark_disconnected(endpoint_id);
            return true;
        }
        return false;
    });

    return something_done;
}
//------------------------------------------------------------------------------
auto router::_is_disconnected(const identifier_t endpoint_id) const noexcept
  -> bool {
    const auto pos = _recently_disconnected.find(endpoint_id);
    return (pos != _recently_disconnected.end()) && !pos->second.is_expired();
}
//------------------------------------------------------------------------------
auto router::_mark_disconnected(const identifier_t endpoint_id) noexcept
  -> void {
    const auto pos = _recently_disconnected.find(endpoint_id);
    if(pos != _recently_disconnected.end()) {
        if(pos->second.is_expired()) {
            _recently_disconnected.erase(pos);
        }
    }
    _recently_disconnected.erase_if(
      [](auto& p) { return std::get<1>(p).is_expired(); });
    _recently_disconnected.emplace(endpoint_id, std::chrono::seconds{15});
}
//------------------------------------------------------------------------------
auto router::_remove_disconnected() noexcept -> work_done {
    some_true something_done{};

    for(auto& [endpoint_id, node] : _nodes) {
        auto& conn = node.the_connection;
        if(node.do_disconnect) [[unlikely]] {
            if(conn) {
                conn->cleanup();
            }
            conn.reset();
        } else {
            if(!conn->is_usable()) [[unlikely]] {
                log_debug("removing disconnected connection").tag("rmDiscConn");
                if(conn) {
                    conn->cleanup();
                }
                conn.reset();
            }
        }
    }
    something_done(_nodes.erase_if([this](auto& p) {
        if(!p.second.the_connection) {
            _mark_disconnected(p.first);
            return true;
        }
        return false;
    }) > 0);

    return something_done;
}
//------------------------------------------------------------------------------
void router::_assign_id(std::unique_ptr<connection>& conn) noexcept {
    assert(conn);
    // find a currently unused endpoint id value
    const auto seq_orig = _id_sequence;
    while(_nodes.find(_id_sequence) != _nodes.end()) {
        if(++_id_sequence >= _id_end) [[unlikely]] {
            _id_sequence = _id_base + 1;
        } else if(_id_sequence == seq_orig) [[unlikely]] {
            return;
        }
    }
    //
    log_debug("assigning id ${id} to accepted ${type} connection")
      .arg("type", conn->type_id())
      .arg("id", _id_sequence);
    // send the special message assigning the endpoint id
    message_view msg{};
    msg.set_target_id(_id_sequence++);
    conn->send(msgbus_id{"assignId"}, msg);
}
//------------------------------------------------------------------------------
void router::_handle_connection(
  std::unique_ptr<connection> a_connection) noexcept {
    assert(a_connection);
    log_info("accepted pending connection")
      .tag("acPendConn")
      .arg("kind", a_connection->kind())
      .arg("type", a_connection->type_id());
    _pending.emplace_back(std::move(a_connection));
}
//------------------------------------------------------------------------------
auto router::_should_log_router_stats() noexcept -> bool {
    return ++_stats.forwarded_messages % 1'000'000 == 0;
}
//------------------------------------------------------------------------------
void router::_log_router_stats() noexcept {
    const auto now{std::chrono::steady_clock::now()};
    const std::chrono::duration<float> interval{now - _forwarded_since_log};

    if(interval > interval.zero()) [[likely]] {
        const auto msgs_per_sec{1'000'000.F / interval.count()};

        log_chart_sample("msgsPerSec", msgs_per_sec);
        log_stat("forwarded ${count} messages")
          .tag("msgStats")
          .arg("count", _stats.forwarded_messages)
          .arg("dropped", _stats.dropped_messages)
          .arg("interval", interval)
          .arg("avgMsgAge", std::chrono::microseconds(_stats.message_age_us))
          .arg("msgsPerSec", msgs_per_sec);
    }

    _forwarded_since_log = now;
}
//------------------------------------------------------------------------------
auto router::_process_blobs() noexcept -> work_done {
    some_true something_done{};
    const auto resend_request =
      [&](message_id msg_id, message_view request) -> bool {
        return this->_route_message(msg_id, _id_base, request);
    };
    something_done(_blobs.update({construct_from, resend_request}));

    if(_blobs.has_outgoing()) {
        for(auto& nd : _nodes) {
            const auto node_id = std::get<0>(nd);
            const auto& conn = std::get<1>(nd).the_connection;
            if(conn && conn->is_usable()) [[likely]] {
                if(auto opt_max_size{conn->max_data_size()}) {
                    const auto handle_send = [node_id, &conn](
                                               message_id msg_id,
                                               const message_view& message) {
                        if(node_id == message.target_id) {
                            return conn->send(msg_id, message);
                        }
                        return false;
                    };
                    if(_blobs.process_outgoing(
                         {construct_from, handle_send},
                         extract(opt_max_size))) {
                        something_done();
                    }
                }
            }
        }
    }
    return something_done;
}
//------------------------------------------------------------------------------
auto router::_do_get_blob_io(
  const message_id msg_id,
  const span_size_t size,
  blob_manipulator& blobs) noexcept -> std::unique_ptr<blob_io> {
    if(is_special_message(msg_id)) {
        if(msg_id.has_method("eptCertPem")) {
            return blobs.make_io(size);
        }
    }
    return {};
}
//------------------------------------------------------------------------------
auto router::_handle_blob(
  const message_id msg_id,
  const message_age,
  const message_view& message) noexcept -> bool {
    // TODO: use message age
    if(is_special_message(msg_id)) {
        if(msg_id.has_method("eptCertPem")) {
            log_trace("received endpoint certificate")
              .arg("source", message.source_id)
              .arg("pem", message.content());
            auto pos = _nodes.find(message.source_id);
            if(pos != _nodes.end()) {
                if(_context->add_remote_certificate_pem(
                     message.source_id, message.content())) {
                    log_debug("verified and stored endpoint certificate")
                      .arg("source", message.source_id);
                }
            }
            if(message.target_id) {
                post_blob(
                  msgbus_id{"eptCertPem"},
                  message.source_id,
                  message.target_id,
                  message.sequence_no,
                  message.content(),
                  adjusted_duration(std::chrono::seconds(30)),
                  message_priority::high);
            }
        }
    }
    return true;
}
//------------------------------------------------------------------------------
auto router::_update_endpoint_info(
  const identifier_t incoming_id,
  const message_view& message) noexcept -> router_endpoint_info& {
    _endpoint_idx[message.source_id] = incoming_id;
    auto& info = _endpoint_infos[message.source_id];
    // sequence_no is the instance id in this message type
    info.assign_instance_id(message);
    return info;
}
//------------------------------------------------------------------------------
auto router::_handle_ping(const message_view& message) noexcept
  -> message_handling_result {
    if(message.target_id == _id_base) {
        message_view response{};
        response.setup_response(message);
        response.set_source_id(_id_base);
        this->_route_message(msgbus_id{"pong"}, _id_base, response);
        return was_handled;
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_subscribed(
  const identifier_t incoming_id,
  const message_view& message) noexcept -> message_handling_result {
    message_id sub_msg_id{};
    if(default_deserialize_message_type(sub_msg_id, message.content()))
      [[likely]] {
        log_debug("endpoint ${source} subscribes to ${message}")
          .arg("source", message.source_id)
          .arg("message", sub_msg_id);

        auto& info = _update_endpoint_info(incoming_id, message);
        message_id_list_add(info.subscriptions, sub_msg_id);
        message_id_list_remove(info.unsubscriptions, sub_msg_id);
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_not_not_a_router(
  const identifier_t incoming_id,
  routed_node& node,
  const message_view& message) noexcept -> message_handling_result {
    if(incoming_id == message.source_id) {
        node.maybe_router = false;
        log_debug("node ${source} is not a router")
          .arg("source", message.source_id);
    }
    return was_handled;
}
//------------------------------------------------------------------------------
auto router::_handle_not_subscribed(
  const identifier_t incoming_id,
  const message_view& message) noexcept -> message_handling_result {
    message_id sub_msg_id{};
    if(default_deserialize_message_type(sub_msg_id, message.content()))
      [[likely]] {
        log_debug("endpoint ${source} unsubscribes from ${message}")
          .arg("source", message.source_id)
          .arg("message", sub_msg_id);

        auto& info = _update_endpoint_info(incoming_id, message);
        message_id_list_remove(info.subscriptions, sub_msg_id);
        message_id_list_add(info.unsubscriptions, sub_msg_id);
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_msg_allow(
  const identifier_t incoming_id,
  routed_node& node,
  const message_view& message) noexcept -> message_handling_result {
    message_id alw_msg_id{};
    if(default_deserialize_message_type(alw_msg_id, message.content())) {
        log_debug("node ${source} allowing message ${message}")
          .arg("message", alw_msg_id)
          .arg("source", message.source_id);
        node.allow_message(alw_msg_id);
        _update_endpoint_info(incoming_id, message);
        return was_handled;
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_msg_block(
  const identifier_t incoming_id,
  routed_node& node,
  const message_view& message) noexcept -> message_handling_result {
    message_id blk_msg_id{};
    if(default_deserialize_message_type(blk_msg_id, message.content())) {
        if(!is_special_message(blk_msg_id)) {
            log_debug("node ${source} blocking message ${message}")
              .arg("message", blk_msg_id)
              .arg("source", message.source_id);
            node.block_message(blk_msg_id);
            _update_endpoint_info(incoming_id, message);
            return was_handled;
        }
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_subscribers_query(const message_view& message) noexcept
  -> message_handling_result {
    const auto pos{_endpoint_infos.find(message.target_id)};
    if(pos != _endpoint_infos.end()) {
        auto& info = pos->second;
        if(info.instance_id) {
            message_id sub_msg_id{};
            if(default_deserialize_message_type(
                 sub_msg_id, message.content())) {
                // if we have the information cached, then respond
                if(message_id_list_contains(info.subscriptions, sub_msg_id)) {
                    message_view response{message.data()};
                    response.setup_response(message);
                    response.set_source_id(message.target_id);
                    response.set_sequence_no(info.instance_id);
                    this->_route_message(
                      msgbus_id{"subscribTo"}, _id_base, response);
                }
                if(message_id_list_contains(info.unsubscriptions, sub_msg_id)) {
                    message_view response{message.data()};
                    response.setup_response(message);
                    response.set_source_id(message.target_id);
                    response.set_sequence_no(info.instance_id);
                    this->_route_message(
                      msgbus_id{"notSubTo"}, _id_base, response);
                }
            }
        }
    }

    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_subscriptions_query(const message_view& message) noexcept
  -> message_handling_result {
    const auto pos = _endpoint_infos.find(message.target_id);
    if(pos != _endpoint_infos.end()) {
        auto& info = pos->second;
        if(info.instance_id) {
            for(auto& sub_msg_id : info.subscriptions) {
                auto temp{default_serialize_buffer_for(sub_msg_id)};
                if(auto serialized{
                     default_serialize_message_type(sub_msg_id, cover(temp))}) {
                    message_view response{extract(serialized)};
                    response.setup_response(message);
                    response.set_source_id(message.target_id);
                    response.set_sequence_no(info.instance_id);
                    this->_route_message(
                      msgbus_id{"subscribTo"}, _id_base, response);
                }
            }
        }
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_router_certificate_query(
  const message_view& message) noexcept -> message_handling_result {
    post_blob(
      msgbus_id{"rtrCertPem"},
      0U,
      message.source_id,
      message.sequence_no,
      _context->get_own_certificate_pem(),
      adjusted_duration(std::chrono::seconds{30}),
      message_priority::high);
    return was_handled;
}
//------------------------------------------------------------------------------
auto router::_handle_endpoint_certificate_query(
  const message_view& message) noexcept -> message_handling_result {
    if(const auto cert_pem{
         _context->get_remote_certificate_pem(message.target_id)}) {
        post_blob(
          msgbus_id{"eptCertPem"},
          message.target_id,
          message.source_id,
          message.sequence_no,
          cert_pem,
          adjusted_duration(std::chrono::seconds{30}),
          message_priority::high);
        return was_handled;
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_topology_query(const message_view& message) noexcept
  -> message_handling_result {
    router_topology_info info{};

    auto temp{default_serialize_buffer_for(info)};
    auto respond = [&](identifier_t remote_id, const auto& conn) {
        info.router_id = _id_base;
        info.remote_id = remote_id;
        info.instance_id = _instance_id;
        info.connect_kind = conn->kind();
        if(const auto serialized{default_serialize(info, cover(temp))})
          [[likely]] {
            message_view response{extract(serialized)};
            response.setup_response(message);
            response.set_source_id(_id_base);
            this->_route_message(msgbus_id{"topoRutrCn"}, _id_base, response);
        }
    };

    for(auto& [nd_id, nd] : this->_nodes) {
        respond(nd_id, nd.the_connection);
    }
    if(_parent_router.confirmed_id) {
        respond(_parent_router.confirmed_id, _parent_router.the_connection);
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_update_stats() noexcept -> work_done {
    some_true something_done;

    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<float> seconds{now - _forwarded_since_stat};
    if(seconds.count() >= 15.F) [[unlikely]] {
        _forwarded_since_stat = now;

        _stats.messages_per_second = static_cast<std::int32_t>(
          float(_stats.forwarded_messages - _prev_forwarded_messages) /
          seconds.count());
        _prev_forwarded_messages = _stats.forwarded_messages;

        const auto avg_msg_age_us = static_cast<std::int32_t>(
          float(std::chrono::duration_cast<std::chrono::microseconds>(
                  _message_age_sum)
                  .count()) /
          float(_stats.forwarded_messages + _stats.dropped_messages + 1));
        const auto avg_msg_age_ms = avg_msg_age_us / 1000;

        _stats.message_age_us = avg_msg_age_us;

        const bool flow_info_changed =
          _flow_info.avg_msg_age_ms != limit_cast<std::int16_t>(avg_msg_age_ms);
        _flow_info.avg_msg_age_ms = limit_cast<std::int16_t>(avg_msg_age_ms);

        if(flow_info_changed) [[unlikely]] {
            const auto send_info =
              [&](const identifier_t remote_id, const auto& conn) {
                  auto buf{default_serialize_buffer_for(_flow_info)};
                  if(const auto serialized{
                       default_serialize(_flow_info, cover(buf))}) [[likely]] {
                      message_view response{extract(serialized)};
                      response.set_source_id(_id_base);
                      response.set_target_id(remote_id);
                      response.set_priority(message_priority::high);
                      conn->send(msgbus_id{"msgFlowInf"}, response);
                      something_done();
                  }
              };

            for(const auto& [nd_id, nd] : this->_nodes) {
                send_info(nd_id, nd.the_connection);
            }
        }
    }
    _stats.uptime_seconds = _uptime_seconds();

    return something_done;
}
//------------------------------------------------------------------------------
auto router::_handle_stats_query(const message_view& message) noexcept
  -> message_handling_result {
    _update_stats();

    auto rs_buf{default_serialize_buffer_for(_stats)};
    if(const auto serialized{default_serialize(_stats, cover(rs_buf))})
      [[likely]] {
        message_view response{extract(serialized)};
        response.setup_response(message);
        response.set_source_id(_id_base);
        this->_route_message(msgbus_id{"statsRutr"}, _id_base, response);
    }

    const auto respond = [&](const identifier_t remote_id, const auto& conn) {
        connection_statistics conn_stats{};
        conn_stats.local_id = _id_base;
        conn_stats.remote_id = remote_id;
        if(conn->query_statistics(conn_stats)) {
            auto cs_buf{default_serialize_buffer_for(conn_stats)};
            if(const auto serialized{
                 default_serialize(conn_stats, cover(cs_buf))}) [[likely]] {
                message_view response{extract(serialized)};
                response.setup_response(message);
                response.set_source_id(_id_base);
                this->_route_message(
                  msgbus_id{"statsConn"}, _id_base, response);
            }
        }
    };

    for(auto& [nd_id, nd] : this->_nodes) {
        respond(nd_id, nd.the_connection);
    }
    if(_parent_router.confirmed_id) [[likely]] {
        respond(_parent_router.confirmed_id, _parent_router.the_connection);
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_bye_bye(
  routed_node& node,
  const message_view& message) noexcept -> message_handling_result {
    if(!node.maybe_router) {
        node.do_disconnect = true;
    }
    _endpoint_idx.erase(message.source_id);
    _endpoint_infos.erase(message.source_id);

    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_blob_fragment(const message_view& message) noexcept
  -> message_handling_result {
    if(_blobs.process_incoming(
         make_callable_ref<&router::_do_get_blob_io>(this), message)) {
        _blobs.fetch_all(make_callable_ref<&router::_handle_blob>(this));
    }
    return (message.target_id == _id_base) ? was_handled : should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_blob_resend(const message_view& message) noexcept
  -> message_handling_result {
    if(message.target_id == _id_base) {
        _blobs.process_resend(message);
        return was_handled;
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_special_common(
  const message_id msg_id,
  const identifier_t incoming_id,
  const message_view& message) noexcept -> message_handling_result {

    switch(msg_id.method_id()) {
        case id_v("ping"):
            return _handle_ping(message);
        case id_v("subscribTo"):
            return _handle_subscribed(incoming_id, message);
        case id_v("unsubFrom"):
        case id_v("notSubTo"):
            return _handle_not_subscribed(incoming_id, message);
        case id_v("qrySubscrb"):
            return _handle_subscribers_query(message);
        case id_v("qrySubscrp"):
            return _handle_subscriptions_query(message);
        case id_v("blobFrgmnt"):
            return _handle_blob_fragment(message);
        case id_v("blobResend"):
            return _handle_blob_resend(message);
        case id_v("rtrCertQry"):
            return _handle_router_certificate_query(message);
        case id_v("eptCertQry"):
            return _handle_endpoint_certificate_query(message);
        case id_v("topoQuery"):
            return _handle_topology_query(message);
        case id_v("statsQuery"):
            return _handle_stats_query(message);
        case id_v("pong"):
        case id_v("topoRutrCn"):
        case id_v("topoBrdgCn"):
        case id_v("topoEndpt"):
        case id_v("statsRutr"):
        case id_v("statsBrdg"):
        case id_v("statsEndpt"):
        case id_v("statsConn"):
            return should_be_forwarded;
        case id_v("requestId"):
        case id_v("msgFlowInf"):
        case id_v("annEndptId"):
            return was_handled;
        [[unlikely]] default:
            log_warning("unhandled special message ${message} from ${source}")
              .tag("unhndldSpc")
              .arg("message", msg_id)
              .arg("source", message.source_id)
              .arg("data", message.data());
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_special(
  const message_id msg_id,
  const identifier_t incoming_id,
  const message_view& message) noexcept -> message_handling_result {
    if(is_special_message(msg_id)) {
        log_debug("router handling special message ${message} from parent")
          .tag("hndlSpcMsg")
          .arg("router", _id_base)
          .arg("message", msg_id)
          .arg("target", message.target_id)
          .arg("source", message.source_id);

        if(msg_id.has_method("stillAlive")) [[unlikely]] {
            _update_endpoint_info(incoming_id, message);
            return should_be_forwarded;
        } else {
            return _handle_special_common(msg_id, incoming_id, message);
        }
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_special(
  const message_id msg_id,
  const identifier_t incoming_id,
  routed_node& node,
  const message_view& message) noexcept -> message_handling_result {
    if(is_special_message(msg_id)) {
        log_debug("router handling special message ${message} from node")
          .arg("router", _id_base)
          .arg("message", msg_id)
          .arg("target", message.target_id)
          .arg("source", message.source_id);

        switch(msg_id.method_id()) {
            case id_v("notARouter"):
                return _handle_not_not_a_router(incoming_id, node, message);
            case id_v("clrBlkList"):
                log_info("clearing router block_list").tag("clrBlkList");
                node.message_block_list.clear();
                return was_handled;
            case id_v("clrAlwList"):
                log_info("clearing router allow_list").tag("clrAlwList");
                node.message_allow_list.clear();
                return was_handled;
            case id_v("stillAlive"):
                _update_endpoint_info(incoming_id, message);
                return should_be_forwarded;
            case id_v("msgAlwList"):
                return _handle_msg_allow(incoming_id, node, message);
            case id_v("msgBlkList"):
                return _handle_msg_block(incoming_id, node, message);
            case id_v("byeByeEndp"):
            case id_v("byeByeRutr"):
            case id_v("byeByeBrdg"):
                log_debug("received bye-bye (${method}) from node ${source}")
                  .arg("method", msg_id.method())
                  .arg("source", message.source_id);
                return _handle_bye_bye(node, message);
            default:
                return _handle_special_common(msg_id, incoming_id, message);
        }
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_use_workers() const noexcept -> bool {
    return _nodes.size() > 2;
}
//------------------------------------------------------------------------------
auto router::_forward_to(
  const routed_node& node_out,
  const message_id msg_id,
  message_view& message) noexcept -> bool {
    if(_should_log_router_stats()) [[unlikely]] {
        _log_router_stats();
    }
    return node_out.send(*this, msg_id, message);
}
//------------------------------------------------------------------------------
auto router::_route_targeted_message(
  const message_id msg_id,
  const identifier_t incoming_id,
  message_view& message) noexcept -> bool {
    bool has_routed = false;
    const auto pos{_endpoint_idx.find(message.target_id)};
    const auto& nodes = this->_nodes;
    if(pos != _endpoint_idx.end()) {
        // if the message should go through the parent router
        if(pos->second == _id_base) {
            has_routed |= _parent_router.send(*this, msg_id, message);
        } else {
            const auto node_pos = nodes.find(pos->second);
            if(node_pos != nodes.end()) {
                auto& node_out = node_pos->second;
                if(node_out.is_allowed(msg_id)) {
                    has_routed = _forward_to(node_out, msg_id, message);
                }
            }
        }
    }

    if(!has_routed) {
        for(const auto& [outgoing_id, node_out] : nodes) {
            if(outgoing_id == message.target_id) {
                if(node_out.is_allowed(msg_id)) {
                    has_routed = _forward_to(node_out, msg_id, message);
                }
            }
        }
    }

    if(!_is_disconnected(message.target_id)) [[likely]] {
        if(!has_routed) {
            for(const auto& [outgoing_id, node_out] : nodes) {
                if(node_out.maybe_router) {
                    if(incoming_id != outgoing_id) {
                        has_routed |= _forward_to(node_out, msg_id, message);
                    }
                }
            }
            // if the message didn't come from the parent router
            if(incoming_id != _id_base) {
                has_routed |= _parent_router.send(*this, msg_id, message);
            }
        }
    }
    return has_routed;
}
//------------------------------------------------------------------------------
auto router::_route_broadcast_message(
  const message_id msg_id,
  const identifier_t incoming_id,
  message_view& message) noexcept -> bool {
    const auto& nodes = this->_nodes;
    for(const auto& [outgoing_id, node_out] : nodes) {
        if(incoming_id != outgoing_id) {
            if(node_out.is_allowed(msg_id)) {
                _forward_to(node_out, msg_id, message);
            }
        }
    }
    if(incoming_id != _id_base) {
        _parent_router.send(*this, msg_id, message);
    }
    return true;
}
//------------------------------------------------------------------------------
auto router::_route_message(
  const message_id msg_id,
  const identifier_t incoming_id,
  message_view& message) noexcept -> bool {

    bool result = true;
    if(!message.too_many_hops()) [[likely]] {
        message.add_hop();

        if(message.target_id != broadcast_endpoint_id()) {
            result |= _route_targeted_message(msg_id, incoming_id, message);
        } else {
            result |= _route_broadcast_message(msg_id, incoming_id, message);
        }
    } else {
        log_warning("message ${message} discarded after too many hops")
          .tag("tooMnyHops")
          .arg("message", msg_id);
        ++_stats.dropped_messages;
    }
    return result;
}
//------------------------------------------------------------------------------
auto router::_route_node_messages(
  const std::chrono::steady_clock::duration message_age_inc,
  const identifier_t incoming_id,
  routed_node& node_in) noexcept -> work_done {
    const auto handler = [&](
                           const message_id msg_id,
                           const message_age msg_age,
                           message_view message) {
        _message_age_sum += message.add_age(msg_age).age() + message_age_inc;
        if(
          this->_handle_special(msg_id, incoming_id, node_in, message) ==
          was_handled) {
            return true;
        }
        if(message.too_old()) [[unlikely]] {
            ++_stats.dropped_messages;
            return true;
        }
        return this->_route_message(msg_id, incoming_id, message);
    };

    const auto& conn_in = node_in.the_connection;
    if(conn_in && conn_in->is_usable()) [[likely]] {
        return conn_in->fetch_messages({construct_from, handler});
    }
    return false;
}
//------------------------------------------------------------------------------
auto router::_route_parent_messages(
  const std::chrono::steady_clock::duration message_age_inc) noexcept
  -> work_done {
    const auto handler = [&](
                           const message_id msg_id,
                           const message_age msg_age,
                           message_view message) {
        _message_age_sum += message.add_age(msg_age).age() + message_age_inc;
        if(
          this->_handle_special(msg_id, _parent_router.confirmed_id, message) ==
          was_handled) {
            return true;
        }
        if(message.too_old()) [[unlikely]] {
            ++_stats.dropped_messages;
            return true;
        }
        return this->_route_message(msg_id, _id_base, message);
    };
    return _parent_router.fetch_messages(*this, handler);
}
//------------------------------------------------------------------------------
auto router::_route_messages() noexcept -> work_done {
    some_true something_done{};
    const auto now{std::chrono::steady_clock::now()};
    const auto message_age_inc{now - _prev_route_time};
    _prev_route_time = now;

    for(auto& entry : _nodes) {
        something_done(_route_node_messages(
          message_age_inc, std::get<0>(entry), std::get<1>(entry)));
    }

    something_done(_route_parent_messages(message_age_inc));

    return something_done;
}
//------------------------------------------------------------------------------
auto router::_update_connections_by_workers(std::latch& completed) noexcept
  -> work_done {
    some_true something_done{};
    auto& workers = main_context().workers();

    for(auto& entry : _nodes) {
        auto& node_in = std::get<1>(entry);
        if(node_in.the_connection) [[likely]] {
            node_in.update_connection = {*node_in.the_connection, completed};
            workers.enqueue(node_in.update_connection);
        }
    }
    something_done(_parent_router.update(*this, _id_base));

    if(!_nodes.empty() || !_pending.empty()) [[likely]] {
        _no_connection_timeout.reset();
    }

    return something_done;
}
//------------------------------------------------------------------------------
auto router::_update_connections_by_router() noexcept -> work_done {
    some_true something_done{};

    for(auto& entry : _nodes) {
        const auto& conn = std::get<1>(entry).the_connection;
        if(conn) [[likely]] {
            something_done(conn->update());
        }
    }
    something_done(_parent_router.update(*this, _id_base));

    if(!_nodes.empty() || !_pending.empty()) [[likely]] {
        _no_connection_timeout.reset();
    } else {
        std::this_thread::yield();
    }
    return something_done;
}
//------------------------------------------------------------------------------
auto router::do_maintenance() noexcept -> work_done {
    some_true something_done{};

    something_done(_update_stats());
    something_done(_process_blobs());
    something_done(_remove_timeouted());
    something_done(_remove_disconnected());

    return something_done;
}
//------------------------------------------------------------------------------
auto router::do_work_by_workers() noexcept -> work_done {
    some_true something_done{};

    something_done(_handle_pending());
    something_done(_route_messages());
    std::latch completed{limit_cast<std::ptrdiff_t>(_nodes.size())};
    something_done(_update_connections_by_workers(completed));
    something_done(_handle_accept());
    completed.wait();

    return something_done;
}
//------------------------------------------------------------------------------
auto router::do_work_by_router() noexcept -> work_done {
    some_true something_done{};

    something_done(_handle_pending());
    something_done(_handle_accept());
    something_done(_route_messages());
    something_done(_update_connections_by_router());

    return something_done;
}
//------------------------------------------------------------------------------
auto router::update(const valid_if_positive<int>& count) noexcept -> work_done {
    some_true something_done{};

    something_done(do_maintenance());

    int n = extract_or(count, 2);
    if(_use_workers()) {
        do {
            something_done(do_work_by_workers());
        } while((n-- > 0) && something_done);
    } else {
        do {
            something_done(do_work_by_router());
        } while((n-- > 0) && something_done);
    }

    return something_done;
}
//------------------------------------------------------------------------------
void router::say_bye() noexcept {
    const auto msgid = msgbus_id{"byeByeRutr"};
    message_view msg{};
    msg.set_source_id(_id_base);
    for(auto& entry : _nodes) {
        const auto& conn = std::get<1>(entry).the_connection;
        if(conn) {
            conn->send(msgid, msg);
            conn->update();
        }
    }

    _parent_router.send(*this, msgid, msg);
}
//------------------------------------------------------------------------------
void router::cleanup() noexcept {
    for(auto& entry : _nodes) {
        const auto& conn = std::get<1>(entry).the_connection;
        if(conn) {
            conn->cleanup();
        }
    }

    log_stat("forwarded ${count} messages in total")
      .tag("msgStats")
      .arg("count", _stats.forwarded_messages)
      .arg("dropped", _stats.dropped_messages)
      .arg("avgMsgAge", std::chrono::microseconds(_stats.message_age_us));
}
//------------------------------------------------------------------------------
void router::finish() noexcept {
    say_bye();
    timeout too_long{adjusted_duration(std::chrono::seconds{1})};
    while(!too_long) {
        update(8);
    }
    cleanup();
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

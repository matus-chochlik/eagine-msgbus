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

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.container;
import eagine.core.utility;
import eagine.core.valid_if;
import eagine.core.runtime;
import eagine.core.main_ctx;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
// helpers
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
    if(not message_id_list_contains(list, entry)) {
        list.push_back(entry);
    }
}
//------------------------------------------------------------------------------
static inline void message_id_list_remove(
  std::vector<message_id>& list,
  const message_id& entry) noexcept {
    const auto pos{std::find(list.begin(), list.end(), entry)};
    if(pos != list.end()) {
        list.erase(pos);
    }
}
//------------------------------------------------------------------------------
// router_pending
//------------------------------------------------------------------------------
router_pending::router_pending(
  router& parent,
  unique_holder<msgbus::connection> conn) noexcept
  : _parent{parent}
  , _connection{std::move(conn)}
  , _connection_type{_connection->type_id()}
  , _connection_kind{_connection->kind()} {
    if(
      parent.password_is_required() and
      (_connection->kind() != connection_kind::in_process)) {
        _nonce.resize(128);
        parent.log_info("password is required on pending ${type} connection")
          .tag("connPwdReq")
          .arg("kind", _connection_kind)
          .arg("type", _connection_type);
    }
}
//------------------------------------------------------------------------------
void router_pending::_assign_id() noexcept {
    auto& parent{_parent.get()};
    if(const auto next_id{parent._get_next_id()}) {
        parent.log_info("assigning id ${id} to accepted ${type} connection")
          .tag("assignId")
          .arg("type", _connection_type)
          .arg("id", next_id);
        // send the special message assigning the endpoint id
        message_view msg{};
        msg.set_source_id(parent.get_id());
        msg.set_target_id(next_id);
        send(msgbus_id{"assignId"}, msg);
    }
}
//------------------------------------------------------------------------------
auto router_pending::assigned_id() const noexcept -> identifier_t {
    return _id;
}
//------------------------------------------------------------------------------
auto router_pending::password_is_required() const noexcept -> bool {
    return not _nonce.empty();
}
//------------------------------------------------------------------------------
auto router_pending::should_request_password() const noexcept -> bool {
    return is_valid_endpoint_id(_id) and password_is_required() and
           _should_request_pwd and not _password_verified;
}
//------------------------------------------------------------------------------
auto router_pending::maybe_router() const noexcept -> bool {
    return _maybe_router;
}
//------------------------------------------------------------------------------
auto router_pending::node_kind() const noexcept -> string_view {
    if(_maybe_router) {
        return "node";
    }
    return "endpoint";
}
//------------------------------------------------------------------------------
auto router_pending::connection_kind() const noexcept {
    return _connection_kind;
}
//------------------------------------------------------------------------------
auto router_pending::connection_type() const noexcept {
    return _connection_type;
}
//------------------------------------------------------------------------------
auto router_pending::has_timeouted() noexcept -> bool {
    return _too_old.is_expired();
}
//------------------------------------------------------------------------------
auto router_pending::should_be_removed() noexcept -> bool {
    return has_timeouted() or not _connection;
}
//------------------------------------------------------------------------------
auto router_pending::can_be_adopted() noexcept -> bool {
    return is_valid_endpoint_id(_id) and not(_too_old) and
           (not password_is_required() or _password_verified);
}
//------------------------------------------------------------------------------
auto router_pending::try_request_password() noexcept -> work_done {
    if(should_request_password()) {
        auto& parent{_parent.get()};
        parent.log_info("requesting router password from ${type} connection")
          .tag("reqRutrPwd")
          .arg("type", _connection_type)
          .arg("id", _id);

        parent.main_context().fill_with_random_bytes(cover(_nonce));
        message_view msg{view(_nonce)};
        msg.set_source_id(parent.get_id());
        msg.set_target_id(_id);
        send(msgbus_id{"reqRutrPwd"}, msg);
        _should_request_pwd.reset();
        return true;
    }
    return false;
}
//------------------------------------------------------------------------------
void router_pending::send(
  const message_id msg_id,
  const message_view& msg) noexcept {
    _connection->send(msg_id, msg);
}
//------------------------------------------------------------------------------
auto router_pending::_handle_msg(
  message_id msg_id,
  message_age,
  const message_view& msg) noexcept -> bool {
    auto& parent{_parent.get()};
    if(is_special_message(msg_id)) {
        // this is a special message requesting endpoint id assignment
        if(msg_id.has_method("requestId")) {
            _assign_id();
            return true;
        }
        // this is a special message containing endpoint id
        if(msg_id.has_method("annEndptId")) {
            _id = msg.source_id;
            _maybe_router = false;
            parent.log_debug("received endpoint id ${id}")
              .tag("annEndptId")
              .arg("id", _id);
            return true;
        }
        // this is a special message containing non-endpoint id
        if(msg_id.has_method("announceId")) {
            _id = msg.source_id;
            parent.log_debug("received id ${id}")
              .tag("announceId")
              .arg("id", _id);
            return true;
        }
        if(msg_id.has_method("encRutrPwd")) {
            if(parent.main_context().matches_encrypted_shared_password(
                 view(_nonce), "msgbus.router.password", msg.data())) {
                parent
                  .log_info(
                    "verified password on pending ${type} connection "
                    "from ${cnterpart} ${id}")
                  .tag("vfyRutrPwd")
                  .arg("cnterpart", node_kind())
                  .arg("type", _connection_type)
                  .arg("id", _id);
                _password_verified = true;
            }
            return true;
        }
    }
    return false;
}
//------------------------------------------------------------------------------
auto router_pending::_msg_handler() noexcept {
    return connection::fetch_handler{
      this, member_function_constant_t<&router_pending::_handle_msg>{}};
}
//------------------------------------------------------------------------------
auto router_pending::update() noexcept -> work_done {
    some_true something_done;
    something_done(_connection->update());
    something_done(_connection->fetch_messages(_msg_handler()));
    something_done(try_request_password());
    something_done(_connection->update());

    return something_done();
}
//------------------------------------------------------------------------------
auto router_pending::release_connection() noexcept
  -> unique_holder<msgbus::connection> {
    return std::move(_connection);
}
//------------------------------------------------------------------------------
// router_endpoint_info
//------------------------------------------------------------------------------
void router_endpoint_info::add_subscription(const message_id msg_id) noexcept {
    message_id_list_add(_subscriptions, msg_id);
    message_id_list_remove(_unsubscriptions, msg_id);
}
//------------------------------------------------------------------------------
void router_endpoint_info::remove_subscription(
  const message_id msg_id) noexcept {
    message_id_list_remove(_subscriptions, msg_id);
    message_id_list_add(_unsubscriptions, msg_id);
}
//------------------------------------------------------------------------------
auto router_endpoint_info::is_subscribed_to(const message_id msg_id) noexcept
  -> bool {
    return message_id_list_contains(_subscriptions, msg_id);
}
//------------------------------------------------------------------------------
auto router_endpoint_info::is_not_subscribed_to(const message_id msg_id) noexcept
  -> bool {
    return message_id_list_contains(_unsubscriptions, msg_id);
}
//------------------------------------------------------------------------------
auto router_endpoint_info::has_instance_id() noexcept -> bool {
    return _instance_id != 0;
}
//------------------------------------------------------------------------------
auto router_endpoint_info::subscriptions() noexcept -> std::vector<message_id> {
    if(has_instance_id()) {
        return _subscriptions;
    }
    return {};
}
//------------------------------------------------------------------------------
auto router_endpoint_info::instance_id() noexcept -> process_instance_id_t {
    return _instance_id;
}
//------------------------------------------------------------------------------
void router_endpoint_info::assign_instance_id(const message_view& msg) noexcept {
    _is_outdated.reset();
    if(_instance_id != msg.sequence_no) {
        _instance_id = msg.sequence_no;
        _subscriptions.clear();
        _unsubscriptions.clear();
    }
}
//------------------------------------------------------------------------------
void router_endpoint_info::apply_instance_id(message_view& msg) noexcept {
    msg.sequence_no = _instance_id;
}
//------------------------------------------------------------------------------
auto router_endpoint_info::is_outdated() const noexcept -> bool {
    return _is_outdated.is_expired();
}
//------------------------------------------------------------------------------
// route_node_messages_work_unit
//------------------------------------------------------------------------------
auto route_node_messages_work_unit::do_it() noexcept -> bool {
    (*_something_done)(
      _node->route_messages(*_parent, _incoming_id, _message_age_inc));
    return true;
}
//------------------------------------------------------------------------------
// connection_update_work_unit
//------------------------------------------------------------------------------
auto connection_update_work_unit::do_it() noexcept -> bool {
    (*_something_done)(_node->do_update_connection());
    return true;
}
//------------------------------------------------------------------------------
// routed_node
//------------------------------------------------------------------------------
routed_node::routed_node() noexcept {
    _message_block_list.reserve(8);
    _message_allow_list.reserve(8);
}
//------------------------------------------------------------------------------
auto routed_node::is_allowed(const message_id msg_id) const noexcept -> bool {
    if(is_special_message(msg_id)) {
        return true;
    }
    const std::shared_lock lk_list{*_lock};
    if(not _message_allow_list.empty()) {
        return message_id_list_contains(_message_allow_list, msg_id);
    }
    if(not _message_block_list.empty()) {
        return not message_id_list_contains(_message_block_list, msg_id);
    }
    return true;
}
//------------------------------------------------------------------------------
void routed_node::setup(
  unique_holder<connection> conn,
  bool maybe_router) noexcept {
    _connection = std::move(conn);
    _maybe_router = maybe_router;
}
//------------------------------------------------------------------------------
void routed_node::enqueue_route_messages(
  workshop& workers,
  router& parent,
  identifier_t incoming_id,
  std::chrono::steady_clock::duration message_age_inc,
  std::latch& completed,
  some_true_atomic& something_done) noexcept {
    if(_connection) [[likely]] {
        _route_messages_work = {
          parent,
          *this,
          incoming_id,
          message_age_inc,
          completed,
          something_done};
        workers.enqueue(_route_messages_work);
    }
}
//------------------------------------------------------------------------------
void routed_node::enqueue_update_connection(
  workshop& workers,
  std::latch& completed,
  some_true_atomic& something_done) noexcept {
    if(_connection) [[likely]] {
        _update_connection_work = {*this, completed, something_done};
        workers.enqueue(_update_connection_work);
    }
}
//------------------------------------------------------------------------------
void routed_node::mark_not_a_router() noexcept {
    const std::unique_lock lk_list{*_lock};
    _maybe_router = false;
}
//------------------------------------------------------------------------------
auto routed_node::do_update_connection() noexcept -> work_done {
    return _connection->update();
}
//------------------------------------------------------------------------------
auto routed_node::update_connection() noexcept -> work_done {
    if(_connection) [[likely]] {
        return _connection->update();
    }
    return false;
}
//------------------------------------------------------------------------------
void routed_node::handle_bye_bye() noexcept {
    const std::unique_lock lk_list{*_lock};
    if(not _maybe_router) {
        _do_disconnect = true;
    }
}
//------------------------------------------------------------------------------
auto routed_node::should_disconnect() const noexcept -> bool {
    return not _connection or _do_disconnect;
}
//------------------------------------------------------------------------------
void routed_node::cleanup_connection() noexcept {
    if(_connection) [[likely]] {
        _connection->cleanup();
        _connection.reset();
        _do_disconnect = false;
    }
}
//------------------------------------------------------------------------------
auto routed_node::kind_of_connection() const noexcept -> connection_kind {
    if(_connection) {
        return _connection->kind();
    }
    return connection_kind::unknown;
}
//------------------------------------------------------------------------------
auto routed_node::query_statistics(connection_statistics& stats) const noexcept
  -> bool {
    if(_connection) {
        return _connection->query_statistics(stats);
    }
    return false;
}
//------------------------------------------------------------------------------
auto routed_node::send(
  const main_ctx_object& user,
  const message_id msg_id,
  const message_view& message) const noexcept -> bool {
    if(_connection) [[likely]] {
        if(not _connection->send(msg_id, message)) [[unlikely]] {
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
auto routed_node::route_messages(
  router& parent,
  const identifier_t incoming_id,
  const std::chrono::steady_clock::duration message_age_inc) noexcept
  -> work_done {

    if(_connection) [[likely]] {
        const auto handler{[&](
                             const message_id msg_id,
                             const message_age msg_age,
                             message_view message) {
            return parent._handle_node_message(
              incoming_id, message_age_inc, msg_id, msg_age, message, *this);
        }};
        return _connection->fetch_messages({construct_from, handler});
    }
    return false;
}
//------------------------------------------------------------------------------
auto routed_node::try_route(
  const main_ctx_object& user,
  const message_id msg_id,
  const message_view& message) const noexcept -> bool {
    if(_maybe_router) {
        return send(user, msg_id, message);
    }
    return false;
}
//------------------------------------------------------------------------------
auto routed_node::process_blobs(
  const identifier_t node_id,
  router_blobs& blobs) noexcept -> work_done {
    some_true something_done;
    if(_connection and _connection->is_usable()) [[likely]] {
        if(auto opt_max_size{_connection->max_data_size()}) {
            const auto handle_send{
              [node_id, this](message_id msg_id, const message_view& message) {
                  if(node_id == message.target_id) {
                      return _connection->send(msg_id, message);
                  }
                  return false;
              }};
            something_done(blobs.process_outgoing(
              {construct_from, handle_send}, *opt_max_size, 4));
        }
    }
    return something_done;
}
//------------------------------------------------------------------------------
void routed_node::block_message(const message_id msg_id) noexcept {
    const std::unique_lock lk_list{*_lock};
    message_id_list_add(_message_block_list, msg_id);
}
//------------------------------------------------------------------------------
void routed_node::allow_message(const message_id msg_id) noexcept {
    const std::unique_lock lk_list{*_lock};
    message_id_list_add(_message_allow_list, msg_id);
}
//------------------------------------------------------------------------------
void routed_node::clear_block_list() noexcept {
    const std::unique_lock lk_list{*_lock};
    _message_block_list.clear();
}
//------------------------------------------------------------------------------
void routed_node::clear_allow_list() noexcept {
    const std::unique_lock lk_list{*_lock};
    _message_allow_list.clear();
}
//------------------------------------------------------------------------------
// parent_router
//------------------------------------------------------------------------------
inline void parent_router::reset(
  unique_holder<connection> a_connection) noexcept {
    _connection = std::move(a_connection);
    _confirmed_id = 0;
}
//------------------------------------------------------------------------------
void parent_router::confirm_id(
  const main_ctx_object& user,
  const message_view& message) noexcept {
    _confirmed_id = message.target_id;
    user.log_debug("confirmed id ${id} by parent router ${source}")
      .tag("confirmdId")
      .arg("id", message.target_id)
      .arg("source", message.source_id);
}
//------------------------------------------------------------------------------
void parent_router::handle_bye(
  const main_ctx_object& user,
  message_id msg_id,
  const message_view& message) const noexcept {
    user
      .log_debug(
        "received bye-bye (${method}) from node ${source} from parent router")
      .tag("handleBye")
      .arg("method", msg_id.method())
      .arg("source", message.source_id);
}
//------------------------------------------------------------------------------
inline void parent_router::announce_id(
  main_ctx_object& user,
  const identifier_t id_base) noexcept {
    message_view announcement{};
    announcement.set_source_id(id_base);
    _connection->send(msgbus_id{"announceId"}, announcement);
    _confirm_id_timeout.reset();

    user.log_debug("announcing id ${id} to parent router")
      .tag("announceId")
      .arg("id", id_base);
}
//------------------------------------------------------------------------------
auto parent_router::kind_of_connection() const noexcept -> connection_kind {
    if(_connection) {
        return _connection->kind();
    }
    return connection_kind::unknown;
}
//------------------------------------------------------------------------------
auto parent_router::query_statistics(connection_statistics& stats) const noexcept
  -> bool {
    if(_connection) {
        return _connection->query_statistics(stats);
    }
    return false;
}
//------------------------------------------------------------------------------
inline auto parent_router::update(
  main_ctx_object& user,
  const identifier_t id_base) noexcept -> work_done {
    const auto exec_time{user.measure_time_interval("parentUpdt")};
    some_true something_done{};

    if(_connection) [[likely]] {
        something_done(_connection->update());
        if(_connection->is_usable()) [[likely]] {
            if(not _confirmed_id) [[unlikely]] {
                if(_confirm_id_timeout) {
                    announce_id(user, id_base);
                    _connection->update();
                    something_done();
                }
            }
        } else {
            if(_confirmed_id) {
                _confirmed_id = 0;
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
    if(_connection) [[likely]] {
        if(not _connection->send(msg_id, message)) [[unlikely]] {
            user.log_debug("failed to send message to parent router");
            return false;
        }
    }
    return true;
}
//------------------------------------------------------------------------------
auto parent_router::route_messages(
  router& parent,
  const std::chrono::steady_clock::duration message_age_inc) noexcept
  -> work_done {

    if(_connection) [[likely]] {
        const auto handler{[&, this](
                             const message_id msg_id,
                             const message_age msg_age,
                             message_view message) {
            return parent._handle_parent_message(
              _confirmed_id, message_age_inc, msg_id, msg_age, message);
        }};
        return _connection->fetch_messages({construct_from, handler});
    }
    return false;
}
//------------------------------------------------------------------------------
// router_nodes
//------------------------------------------------------------------------------
auto router_nodes::get() noexcept {
    return cover(_nodes);
}
//------------------------------------------------------------------------------
auto router_nodes::count() noexcept -> std::size_t {
    return _nodes.size();
}
//------------------------------------------------------------------------------
auto router_nodes::has_id(const identifier_t id) noexcept -> bool {
    return _nodes.contains(id);
}
//------------------------------------------------------------------------------
auto router_nodes::find(const identifier_t id) noexcept
  -> optional_reference<routed_node> {
    return eagine::find(_nodes, id).ref();
}
//------------------------------------------------------------------------------
auto router_nodes::find_outgoing(const identifier_t target_id)
  -> valid_endpoint_id {
    return eagine::find(_endpoint_idx, target_id)
      .value_or(invalid_endpoint_id());
}
//------------------------------------------------------------------------------
auto router_nodes::has_some() noexcept -> bool {
    return not _nodes.empty() or not _pending.empty();
}
//------------------------------------------------------------------------------
void router_nodes::add_acceptor(shared_holder<acceptor> an_acceptor) noexcept {
    _acceptors.emplace_back(std::move(an_acceptor));
}
//------------------------------------------------------------------------------
void router_nodes::_adopt_pending(
  router& parent,
  router_pending& pending) noexcept {
    const auto id{pending.assigned_id()};
    parent.log_info("adopting pending connection from ${cnterpart} ${id}")
      .tag("adPendConn")
      .arg("kind", pending.connection_kind())
      .arg("type", pending.connection_type())
      .arg("cnterpart", pending.node_kind())
      .arg("id", id);

    // send the special message confirming assigned endpoint id
    message_view confirmation{};
    confirmation.set_source_id(parent.get_id()).set_target_id(id);
    pending.send(msgbus_id{"confirmId"}, confirmation);

    auto node{eagine::find(_nodes, id)};
    if(not node) {
        node.try_emplace(id);
        parent._update_use_workers();
    }
    node->setup(pending.release_connection(), pending.maybe_router());
    _recently_disconnected.erase(id);
}
//------------------------------------------------------------------------------
auto router_nodes::_do_handle_pending(router& parent) noexcept -> work_done {
    some_true something_done{};

    std::size_t idx = 0;
    while(idx < _pending.size()) {
        auto& pending = _pending[idx];
        if(pending.should_be_removed()) {
            _pending.erase(_pending.begin() + signedness_cast(idx));
            something_done();
        } else {
            something_done(pending.update());
            if(pending.can_be_adopted()) {
                _adopt_pending(parent, pending);
                something_done();
            }
            ++idx;
        }
    }
    return something_done;
}
//------------------------------------------------------------------------------
auto router_nodes::handle_pending(router& parent) noexcept -> work_done {

    if(not _pending.empty()) [[unlikely]] {
        return _do_handle_pending(parent);
    }
    return false;
}
//------------------------------------------------------------------------------
auto router_nodes::handle_accept(router& parent) noexcept -> work_done {
    some_true something_done{};

    if(not _acceptors.empty()) [[likely]] {
        const auto handle_conn{[&](unique_holder<connection> a_connection) {
            assert(a_connection);
            parent.log_info("accepted pending connection")
              .tag("acPendConn")
              .arg("kind", a_connection->kind())
              .arg("type", a_connection->type_id());
            _pending.emplace_back(parent, std::move(a_connection));
        }};
        acceptor::accept_handler handler{construct_from, handle_conn};
        for(auto& an_acceptor : _acceptors) {
            assert(an_acceptor);
            something_done(an_acceptor->update());
            something_done(an_acceptor->process_accepted(handler));
        }
    }
    return something_done;
}
//------------------------------------------------------------------------------
auto router_nodes::remove_timeouted(const main_ctx_object& user) noexcept
  -> work_done {
    some_true something_done{};

    std::erase_if(_pending, [&, this](auto& pending) {
        if(pending.has_timeouted()) {
            something_done();
            user.log_warning("removing timeouted pending ${type} connection")
              .tag("rmPendConn")
              .arg("type", pending.connection_type());
            return true;
        }
        return false;
    });

    _endpoint_infos.erase_if([this](auto& entry) {
        auto& [endpoint_id, info] = entry;
        if(info.is_outdated()) {
            _endpoint_idx.erase(endpoint_id);
            mark_disconnected(endpoint_id);
            return true;
        }
        return false;
    });

    return something_done;
}
//------------------------------------------------------------------------------
auto router_nodes::is_disconnected(const identifier_t endpoint_id) const noexcept
  -> bool {
    return eagine::find(_recently_disconnected, endpoint_id)
      .transform([](auto& node) { return not node.is_expired(); })
      .or_false();
}
//------------------------------------------------------------------------------
void router_nodes::mark_disconnected(const identifier_t endpoint_id) noexcept {
    const auto node{eagine::find(_recently_disconnected, endpoint_id)};
    if(node) {
        if(node->is_expired()) {
            _recently_disconnected.erase(node.position());
        }
    }
    _recently_disconnected.erase_if(
      [&](auto& p) { return p.second.is_expired(); });
    _recently_disconnected.emplace(endpoint_id, std::chrono::seconds{15});
}
//------------------------------------------------------------------------------
auto router_nodes::remove_disconnected(const main_ctx_object& user) noexcept
  -> work_done {
    some_true something_done{};

    for(auto& [node_id, node] : _nodes) {
        if(node.should_disconnect()) [[unlikely]] {
            user.log_debug("removing disconnected connection")
              .tag("rmDiscConn");
            node.cleanup_connection();
        }
    }
    something_done(_nodes.erase_if([this](auto& p) {
        if(p.second.should_disconnect()) [[unlikely]] {
            mark_disconnected(p.first);
            return true;
        }
        return false;
    }) > 0);

    return something_done;
}
//------------------------------------------------------------------------------
auto router_nodes::update_endpoint_info(
  const identifier_t incoming_id,
  const message_view& message) noexcept -> router_endpoint_info& {
    _endpoint_idx[message.source_id] = incoming_id;
    auto& info = _endpoint_infos[message.source_id];
    info.assign_instance_id(message);
    return info;
}
//------------------------------------------------------------------------------
auto router_nodes::subscribes_to(
  const identifier_t target_id,
  const message_id sub_msg_id) noexcept
  -> std::tuple<tribool, tribool, process_instance_id_t> {

    if(const auto info{eagine::find(_endpoint_infos, target_id)}) {
        if(info->has_instance_id()) {
            return {
              info->is_subscribed_to(sub_msg_id),
              info->is_not_subscribed_to(sub_msg_id),
              info->instance_id()};
        }
    }
    return {indeterminate, indeterminate, 0U};
}
//------------------------------------------------------------------------------
auto router_nodes::subscriptions_of(const identifier_t target_id) noexcept
  -> std::tuple<std::vector<message_id>, process_instance_id_t> {
    if(const auto info{eagine::find(_endpoint_infos, target_id)}) {
        return {info->subscriptions(), info->instance_id()};
    }
    return {{}, 0U};
}
//------------------------------------------------------------------------------
void router_nodes::erase(const identifier_t id) noexcept {
    _endpoint_idx.erase(id);
    _endpoint_infos.erase(id);
}
//------------------------------------------------------------------------------
void router_nodes::cleanup() noexcept {
    for(auto& entry : _nodes) {
        std::get<1>(entry).cleanup_connection();
    }
}
//------------------------------------------------------------------------------
// router_stats
//------------------------------------------------------------------------------
auto router_stats::uptime() noexcept -> std::chrono::seconds {
    return std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::steady_clock::now() - _startup_time);
}
//------------------------------------------------------------------------------
auto router_stats::time_since_last_routing() noexcept -> auto {
    const auto now{std::chrono::steady_clock::now()};
    const auto message_age_inc{now - _prev_route_time};
    _prev_route_time = now;
    return message_age_inc;
}
//------------------------------------------------------------------------------
void router_stats::update_avg_msg_age(
  const std::chrono::steady_clock::duration message_age_inc) noexcept {
    const std::unique_lock lk{_lock};
    _message_age_avg.add(message_age_inc);
}
//------------------------------------------------------------------------------
auto router_stats::avg_msg_age() noexcept -> std::chrono::microseconds {
    const std::shared_lock lk{_lock};
    return std::chrono::duration_cast<std::chrono::microseconds>(
      _message_age_avg.get());
}
//------------------------------------------------------------------------------
auto router_stats::statistics() noexcept -> router_statistics {
    return _stats;
}
//------------------------------------------------------------------------------
auto router_stats::update_stats() noexcept
  -> std::tuple<std::optional<message_flow_info>> {

    const auto now{std::chrono::steady_clock::now()};
    const std::chrono::duration<float> seconds{now - _forwarded_since_stat};
    _stats.uptime_seconds = uptime().count();

    if(seconds >= std::chrono::seconds{15}) [[unlikely]] {
        _forwarded_since_stat = now;

        _stats.messages_per_second = static_cast<std::int32_t>(
          float(_stats.forwarded_messages - _prev_forwarded_messages) /
          seconds.count());
        _prev_forwarded_messages = _stats.forwarded_messages;

        const auto avg_msg_age_us =
          static_cast<std::int32_t>(avg_msg_age().count() + 500);
        const auto avg_msg_age_ms = avg_msg_age_us / 1000;

        _stats.message_age_us = avg_msg_age_us;

        const bool flow_info_changed{
          _flow_info.avg_msg_age_ms != avg_msg_age_ms};
        _flow_info.set_average_message_age(
          std::chrono::milliseconds{avg_msg_age_ms});
        if(flow_info_changed) {
            return {{_flow_info}};
        }
    }

    return {};
}
//------------------------------------------------------------------------------
void router_stats::message_dropped() noexcept {
    ++_stats.dropped_messages;
}
//------------------------------------------------------------------------------
void router_stats::log_stats(const main_ctx_object& user) noexcept {
    if(++_stats.forwarded_messages % 1'000'000 == 0) {
        const auto now{std::chrono::steady_clock::now()};
        const std::chrono::duration<float> interval{now - _forwarded_since_log};
        _forwarded_since_log = now;

        if(interval > interval.zero()) [[likely]] {
            const auto msgs_per_sec{1'000'000.F / interval.count()};

            user.log_chart_sample("msgsPerSec", msgs_per_sec);
            user.log_stat("forwarded ${count} messages")
              .tag("msgStats")
              .arg("count", _stats.forwarded_messages)
              .arg("dropped", _stats.dropped_messages)
              .arg("interval", interval)
              .arg("avgMsgAge", avg_msg_age())
              .arg("msgsPerSec", msgs_per_sec);
        }
    }
}
//------------------------------------------------------------------------------
// router_ids
//------------------------------------------------------------------------------
auto router_ids::router_id() noexcept -> identifier_t {
    return _id_base;
}
//------------------------------------------------------------------------------
auto router_ids::instance_id() noexcept -> process_instance_id_t {
    return _instance_id;
}
//------------------------------------------------------------------------------
void router_ids::set_description(main_ctx_object& user) {
    using std::to_string;
    user.object_description(
      "Router-" + to_string(_id_base),
      "Message bus router id " + to_string(_id_base));
}
//------------------------------------------------------------------------------
void router_ids::setup_from_config(const main_ctx_object& user) {

    const auto id_count{user.app_config()
                          .get<host_id_t>("msgbus.router.id_count")
                          .value_or(1U << 12U)};

    const auto host_id{
      identifier_t(user.main_context().system().host_id().value_or(0U))};

    _id_base =
      user.app_config()
        .get<identifier_t>("msgbus.router.id_major")
        .value_or(host_id << 32U) +
      user.app_config().get<identifier_t>("msgbus.router.id_minor").value_or(0U);

    if(_id_base) {
        _id_end = _id_base + id_count;
    } else {
        _id_base = 1;
        _id_end = id_count;
    }
    _id_sequence = _id_base + 1;

    user.log_info("using router id range ${base} - ${end} (${count})")
      .tag("idRange")
      .arg("count", id_count)
      .arg("base", _id_base)
      .arg("end", _id_end);
}
//------------------------------------------------------------------------------
auto router_ids::get_next_id(router& parent) noexcept -> identifier_t {
    const auto seq_orig = _id_sequence;
    while(parent.has_node_id(_id_sequence)) {
        if(++_id_sequence >= _id_end) [[unlikely]] {
            _id_sequence = _id_base + 1;
        } else if(_id_sequence == seq_orig) [[unlikely]] {
            return 0;
        }
    }
    return _id_sequence++;
}
//------------------------------------------------------------------------------
// router context
//------------------------------------------------------------------------------
router_context::router_context(shared_context context) noexcept
  : _context{std::move(context)} {}
//------------------------------------------------------------------------------
auto router_context::add_certificate_pem(const memory::const_block blk) noexcept
  -> bool {
    if(_context) [[likely]] {
        const std::unique_lock lk_ctx{_context_lock};
        _context->add_own_certificate_pem(blk);
    }
    return false;
}
//------------------------------------------------------------------------------
auto router_context::add_ca_certificate_pem(
  const memory::const_block blk) noexcept -> bool {
    if(_context) [[likely]] {
        const std::unique_lock lk_ctx{_context_lock};
        _context->add_ca_certificate_pem(blk);
    }
    return false;
}
//------------------------------------------------------------------------------
auto router_context::add_remote_certificate_pem(
  const identifier_t id,
  const memory::const_block blk) noexcept -> bool {
    if(_context) [[likely]] {
        const std::unique_lock lk_ctx{_context_lock};
        return _context->add_remote_certificate_pem(id, blk);
    }
    return false;
}
//------------------------------------------------------------------------------
auto router_context::get_own_certificate_pem() noexcept -> memory::const_block {
    if(_context) [[likely]] {
        const std::unique_lock lk_ctx{_context_lock};
        return _context->get_own_certificate_pem();
    }
    return {};
}
//------------------------------------------------------------------------------
auto router_context::get_remote_certificate_pem(const identifier_t id) noexcept
  -> memory::const_block {
    if(_context) [[likely]] {
        const std::unique_lock lk_ctx{_context_lock};
        return _context->get_remote_certificate_pem(id);
    }
    return {};
}
//------------------------------------------------------------------------------
// router_blobs
//------------------------------------------------------------------------------
router_blobs::router_blobs(router& parent) noexcept
  : _blobs{parent, msgbus_id{"blobFrgmnt"}, msgbus_id{"blobResend"}} {}
//------------------------------------------------------------------------------
auto router_blobs::has_outgoing() noexcept -> bool {
    return _blobs.has_outgoing();
}
//------------------------------------------------------------------------------
auto router_blobs::process_outgoing(
  const send_handler handle_send,
  const span_size_t max_data_size,
  span_size_t max_messages) noexcept -> work_done {
    return _blobs.process_outgoing(handle_send, max_data_size, max_messages);
}
//------------------------------------------------------------------------------
void router_blobs::push_outgoing(
  const message_id msg_id,
  const identifier_t source_id,
  const identifier_t target_id,
  const blob_id_t target_blob_id,
  const memory::const_block blob,
  const std::chrono::seconds max_time,
  const message_priority priority) noexcept {
    _blobs.push_outgoing(
      msg_id, source_id, target_id, target_blob_id, blob, max_time, priority);
}
//------------------------------------------------------------------------------
auto router_blobs::process_blobs(
  const identifier_t parent_id,
  router& parent) noexcept -> work_done {
    some_true something_done{_blobs.handle_complete() > 0};
    const auto resend_request{
      [&](message_id msg_id, message_view request) -> bool {
          return parent._route_message(msg_id, parent_id, request);
      }};
    something_done(_blobs.update(
      {construct_from, resend_request}, min_connection_data_size));

    return something_done;
}
//------------------------------------------------------------------------------
auto router_blobs::_get_blob_target_io(
  const message_id msg_id,
  const span_size_t size,
  blob_manipulator& blobs) noexcept -> unique_holder<target_blob_io> {
    if(is_special_message(msg_id)) {
        if(msg_id.has_method("eptCertPem")) {
            return blobs.make_target_io(size);
        }
    }
    return {};
}
//------------------------------------------------------------------------------
void router_blobs::handle_fragment(
  const message_view& message,
  const fetch_handler handle_fetch) noexcept {
    if(_blobs.process_incoming(
         make_callable_ref<&router_blobs::_get_blob_target_io>(this),
         message)) {
        _blobs.fetch_all(handle_fetch);
    }
}
//------------------------------------------------------------------------------
void router_blobs::handle_resend(const message_view& message) noexcept {
    _blobs.process_resend(message);
}
//------------------------------------------------------------------------------
// router
//------------------------------------------------------------------------------
router::router(main_ctx_parent parent) noexcept
  : main_ctx_object{"MsgBusRutr", parent}
  , _context{make_context(*this)}
  , _password_is_required{app_config()
                            .get<bool>("msgbus.router.requires_password")
                            .value_or(false)} {
    _ids.setup_from_config(*this);
    _ids.set_description(*this);
}
//------------------------------------------------------------------------------
auto router::_uptime_seconds() noexcept -> std::int64_t {
    return _stats.uptime().count();
}
//------------------------------------------------------------------------------
auto router::get_id() noexcept -> identifier_t {
    return _ids.router_id();
}
//------------------------------------------------------------------------------
auto router::has_id(const identifier_t id) noexcept -> bool {
    return get_id() == id;
}
//------------------------------------------------------------------------------
auto router::has_node_id(const identifier_t id) noexcept -> bool {
    return _nodes.has_id(id);
}
//------------------------------------------------------------------------------
auto router::node_count() noexcept -> span_size_t {
    return _nodes.count();
}
//------------------------------------------------------------------------------
auto router::password_is_required() const noexcept -> bool {
    return _password_is_required;
}
//------------------------------------------------------------------------------
void router::add_certificate_pem(const memory::const_block blk) noexcept {
    _context.add_certificate_pem(blk);
}
//------------------------------------------------------------------------------
void router::add_ca_certificate_pem(const memory::const_block blk) noexcept {
    _context.add_ca_certificate_pem(blk);
}
//------------------------------------------------------------------------------
auto router::add_acceptor(shared_holder<acceptor> an_acceptor) noexcept
  -> bool {
    if(an_acceptor) {
        log_info("adding connection acceptor")
          .tag("addAccptor")
          .arg("kind", an_acceptor->kind())
          .arg("type", an_acceptor->type_id());
        _nodes.add_acceptor(std::move(an_acceptor));
        return true;
    }
    return false;
}
//------------------------------------------------------------------------------
auto router::add_connection(unique_holder<connection> a_connection) noexcept
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
auto router::_remove_disconnected() noexcept -> work_done {
    some_true something_done{_nodes.remove_disconnected(*this)};
    if(something_done) {
        _update_use_workers();
    }
    return something_done;
}
//------------------------------------------------------------------------------
auto router::_get_next_id() noexcept -> identifier_t {
    return _ids.get_next_id(*this);
}
//------------------------------------------------------------------------------
auto router::_process_blobs() noexcept -> work_done {
    some_true something_done{_blobs.process_blobs(get_id(), *this)};

    if(_blobs.has_outgoing()) {
        for(auto& [node_id, node] : _nodes.get()) {
            something_done(node.process_blobs(node_id, _blobs));
        }
    }
    return something_done;
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
            if(has_node_id(message.source_id)) {
                if(_context.add_remote_certificate_pem(
                     message.source_id, message.content())) {
                    log_debug("verified and stored endpoint certificate")
                      .arg("source", message.source_id);
                }
            }
            if(message.target_id) {
                _blobs.push_outgoing(
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
    const std::unique_lock lk{_router_lock};
    return _nodes.update_endpoint_info(incoming_id, message);
}
//------------------------------------------------------------------------------
auto router::_send_flow_info(const message_flow_info& flow_info) noexcept
  -> work_done {
    const auto send_info{[&](const identifier_t remote_id, auto& node) {
        auto buf{default_serialize_buffer_for(flow_info)};
        if(const auto serialized{default_serialize(flow_info, cover(buf))})
          [[likely]] {
            message_view response{*serialized};
            response.set_source_id(get_id());
            response.set_target_id(remote_id);
            response.set_priority(message_priority::high);
            node.send(*this, msgbus_id{"msgFlowInf"}, response);
        }
    }};

    for(const auto& [node_id, node] : _nodes.get()) {
        send_info(node_id, node);
    }
    return _nodes.count() > 0;
}
//------------------------------------------------------------------------------
auto router::_handle_ping(const message_view& message) noexcept
  -> message_handling_result {
    const auto own_id{get_id()};
    if(message.target_id == own_id) {
        message_view response{};
        response.setup_response(message);
        response.set_source_id(own_id);
        this->_route_message(msgbus_id{"pong"}, own_id, response);
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
        const std::unique_lock lk{_router_lock};
        info.add_subscription(sub_msg_id);
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_clear_block_list(routed_node& node) noexcept
  -> message_handling_result {
    log_info("clearing router block_list").tag("clrBlkList");
    node.clear_block_list();
    return was_handled;
}
//------------------------------------------------------------------------------
auto router::_handle_clear_allow_list(routed_node& node) noexcept
  -> message_handling_result {
    log_info("clearing router allow_list").tag("clrAlwList");
    node.clear_allow_list();
    return was_handled;
}
//------------------------------------------------------------------------------
auto router::_handle_still_alive(
  const identifier_t incoming_id,
  const message_view& message) noexcept -> message_handling_result {
    _update_endpoint_info(incoming_id, message);
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_not_not_a_router(
  const identifier_t incoming_id,
  routed_node& node,
  const message_view& message) noexcept -> message_handling_result {
    if(incoming_id == message.source_id) {
        log_debug("node ${source} is not a router")
          .arg("source", message.source_id);
        const std::unique_lock lk{_router_lock};
        node.mark_not_a_router();
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
        const std::unique_lock lk{_router_lock};
        info.remove_subscription(sub_msg_id);
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
        if(not is_special_message(blk_msg_id)) {
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
    message_id sub_msg_id{};
    if(default_deserialize_message_type(sub_msg_id, message.content())) {
        const auto [is_sub, is_not_sub, inst_id] = [&, this] {
            const std::unique_lock lk{_router_lock};
            return _nodes.subscribes_to(message.target_id, sub_msg_id);
        }();
        if(is_sub or is_not_sub) {
            const auto own_id{get_id()};
            message_view response{message.data()};
            response.setup_response(message);
            response.set_source_id(message.target_id);
            response.sequence_no = inst_id;
            if(is_sub) {
                _route_message(msgbus_id{"subscribTo"}, own_id, response);
            }
            if(is_not_sub) {
                _route_message(msgbus_id{"notSubTo"}, own_id, response);
            }
        }
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_subscriptions_query(const message_view& message) noexcept
  -> message_handling_result {
    const auto [subs, inst_id] = [&, this] {
        const std::unique_lock lk{_router_lock};
        return _nodes.subscriptions_of(message.target_id);
    }();
    for(const auto& sub_msg_id : subs) {
        auto temp{default_serialize_buffer_for(sub_msg_id)};
        if(auto serialized{
             default_serialize_message_type(sub_msg_id, cover(temp))}) {
            message_view response{*serialized};
            response.setup_response(message);
            response.set_source_id(message.target_id);
            response.sequence_no = inst_id;
            _route_message(msgbus_id{"subscribTo"}, get_id(), response);
        }
    }

    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_password_request(const message_view& message) noexcept
  -> message_handling_result {
    const std::unique_lock lk{_router_lock};
    if(has_id(message.target_id)) {
        memory::buffer encrypted;
        if(main_context().encrypt_shared_password(
             message.data(), "msgbus.router.password", encrypted)) {
            message_view response{view(encrypted)};
            response.setup_response(message);
            _parent_router.send(*this, msgbus_id{"encRutrPwd"}, response);
        }
    }
    return was_handled;
}
//------------------------------------------------------------------------------
auto router::_handle_router_certificate_query(
  const message_view& message) noexcept -> message_handling_result {
    const std::unique_lock lk{_router_lock};
    _blobs.push_outgoing(
      msgbus_id{"rtrCertPem"},
      0U,
      message.source_id,
      message.sequence_no,
      _context.get_own_certificate_pem(),
      adjusted_duration(std::chrono::seconds{30}),
      message_priority::high);
    return was_handled;
}
//------------------------------------------------------------------------------
auto router::_handle_endpoint_certificate_query(
  const message_view& message) noexcept -> message_handling_result {
    const std::unique_lock lk{_router_lock};
    if(const auto cert_pem{
         _context.get_remote_certificate_pem(message.target_id)}) {
        _blobs.push_outgoing(
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

    const auto own_id{get_id()};
    router_topology_info info{
      .router_id = own_id, .instance_id = _ids.instance_id()};

    auto temp{default_serialize_buffer_for(info)};
    const auto respond{
      [&](identifier_t remote_id, const connection_kind conn_kind) {
          info.remote_id = remote_id;
          info.connect_kind = conn_kind;
          if(const auto serialized{default_serialize(info, cover(temp))})
            [[likely]] {
              message_view response{*serialized};
              response.setup_response(message);
              response.set_source_id(own_id);
              this->_route_message(msgbus_id{"topoRutrCn"}, own_id, response);
          }
      }};

    for(auto& [nd_id, nd] : _nodes.get()) {
        respond(nd_id, nd.kind_of_connection());
    }
    if(_parent_router) {
        respond(_parent_router.id(), _parent_router.kind_of_connection());
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_update_stats() noexcept -> work_done {
    some_true something_done{};
    const auto [new_flow_info] = _stats.update_stats();
    if(new_flow_info) {
        something_done(_send_flow_info(*new_flow_info));
    }
    return something_done;
}
//------------------------------------------------------------------------------
auto router::_handle_stats_query(const message_view& message) noexcept
  -> message_handling_result {

    const auto own_id{get_id()};
    const auto stats{_stats.statistics()};
    auto rs_buf{default_serialize_buffer_for(stats)};
    if(const auto serialized{default_serialize(stats, cover(rs_buf))})
      [[likely]] {
        message_view response{*serialized};
        response.setup_response(message);
        response.set_source_id(own_id);
        this->_route_message(msgbus_id{"statsRutr"}, own_id, response);
    }

    const auto respond{[&, this](
                         const identifier_t remote_id, const auto& node) {
        connection_statistics conn_stats{};
        conn_stats.local_id = own_id;
        conn_stats.remote_id = remote_id;
        if(node.query_statistics(conn_stats)) {
            auto cs_buf{default_serialize_buffer_for(conn_stats)};
            if(const auto serialized{
                 default_serialize(conn_stats, cover(cs_buf))}) [[likely]] {
                message_view response{*serialized};
                response.setup_response(message);
                response.set_source_id(own_id);
                this->_route_message(msgbus_id{"statsConn"}, own_id, response);
            }
        }
    }};

    for(auto& [node_id, node] : _nodes.get()) {
        respond(node_id, node);
    }
    if(_parent_router) [[likely]] {
        respond(_parent_router.id(), _parent_router);
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_bye_bye(
  const message_id msg_id,
  routed_node& node,
  const message_view& message) noexcept -> message_handling_result {
    log_debug("received bye-bye (${method}) from node ${source}")
      .arg("method", msg_id.method())
      .arg("source", message.source_id);

    node.handle_bye_bye();

    const std::unique_lock lk{_router_lock};
    _nodes.erase(message.source_id);

    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_blob_fragment(const message_view& message) noexcept
  -> message_handling_result {

    const std::unique_lock lk{_router_lock};
    _blobs.handle_fragment(
      message, make_callable_ref<&router::_handle_blob>(this));
    return has_id(message.target_id) ? was_handled : should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_handle_blob_resend(const message_view& message) noexcept
  -> message_handling_result {
    if(has_id(message.target_id)) {
        const std::unique_lock lk{_router_lock};
        _blobs.handle_resend(message);
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
        case id_v("reqRutrPwd"):
            return _handle_password_request(message);
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
            log_warning(
              "unhandled special message ${message} from "
              "${source}")
              .tag("unhndldSpc")
              .arg("message", msg_id)
              .arg("source", message.source_id)
              .arg("data", message.data());
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_do_handle_special(
  const message_id msg_id,
  const identifier_t incoming_id,
  const message_view& message) noexcept -> message_handling_result {
    log_debug("router handling special message ${message} from parent")
      .tag("hndlSpcMsg")
      .arg("router", get_id())
      .arg("message", msg_id)
      .arg("target", message.target_id)
      .arg("source", message.source_id);

    if(not msg_id.has_method("stillAlive")) [[likely]] {
        return _handle_special_common(msg_id, incoming_id, message);
    } else {
        _update_endpoint_info(incoming_id, message);
        return should_be_forwarded;
    }
}
//------------------------------------------------------------------------------
inline auto router::_handle_special(
  const message_id msg_id,
  const identifier_t incoming_id,
  const message_view& message) noexcept -> message_handling_result {
    if(is_special_message(msg_id)) {
        return _do_handle_special(msg_id, incoming_id, message);
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
auto router::_do_handle_special(
  const message_id msg_id,
  const identifier_t incoming_id,
  routed_node& node,
  const message_view& message) noexcept -> message_handling_result {
    log_debug("router handling special message ${message} from node")
      .arg("router", get_id())
      .arg("message", msg_id)
      .arg("target", message.target_id)
      .arg("source", message.source_id);

    switch(msg_id.method_id()) {
        case id_v("notARouter"):
            return _handle_not_not_a_router(incoming_id, node, message);
        case id_v("clrBlkList"):
            return _handle_clear_block_list(node);
        case id_v("clrAlwList"):
            return _handle_clear_allow_list(node);
        case id_v("stillAlive"):
            return _handle_still_alive(incoming_id, message);
        case id_v("msgAlwList"):
            return _handle_msg_allow(incoming_id, node, message);
        case id_v("msgBlkList"):
            return _handle_msg_block(incoming_id, node, message);
        case id_v("byeByeEndp"):
        case id_v("byeByeRutr"):
        case id_v("byeByeBrdg"):
            return _handle_bye_bye(msg_id, node, message);
        default:
            return _handle_special_common(msg_id, incoming_id, message);
    }
}
//------------------------------------------------------------------------------
inline auto router::_handle_special(
  const message_id msg_id,
  const identifier_t incoming_id,
  routed_node& node,
  const message_view& message) noexcept -> message_handling_result {
    if(is_special_message(msg_id)) {
        return _do_handle_special(msg_id, incoming_id, node, message);
    }
    return should_be_forwarded;
}
//------------------------------------------------------------------------------
void router::_update_use_workers() noexcept {
    const bool used_workers{_use_worker_threads};
    _use_worker_threads = node_count() > 2;
    if(used_workers and not _use_worker_threads) {
        log_info("switching to single-threaded mode").tag("singleThrd");
    } else if(not used_workers and _use_worker_threads) {
        log_info("switching to multi-threaded mode").tag("multiThrd");
    }
}
//------------------------------------------------------------------------------
auto router::_forward_to(
  const routed_node& node_out,
  const message_id msg_id,
  message_view& message) noexcept -> bool {
    _stats.log_stats(*this);
    return node_out.send(*this, msg_id, message);
}
//------------------------------------------------------------------------------
auto router::_route_targeted_message(
  const message_id msg_id,
  const identifier_t incoming_id,
  message_view& message) noexcept -> bool {
    bool has_routed = false;

    const auto own_id{get_id()};
    if(const auto outgoing_id{_nodes.find_outgoing(message.target_id)}) {
        // if the message should go through the parent router
        if(*outgoing_id == own_id) {
            const std::unique_lock lk{_router_lock};
            has_routed |= _parent_router.send(*this, msg_id, message);
        } else {
            _nodes.find(*outgoing_id).and_then([&](auto& node_out) {
                if(node_out.is_allowed(msg_id)) {
                    const std::unique_lock lk{_router_lock};
                    has_routed = _forward_to(node_out, msg_id, message);
                }
            });
        }
    }

    if(not has_routed) {
        for(const auto& [outgoing_id, node_out] : _nodes.get()) {
            if(outgoing_id == message.target_id) {
                if(node_out.is_allowed(msg_id)) {
                    const std::unique_lock lk{_router_lock};
                    has_routed = _forward_to(node_out, msg_id, message);
                }
            }
        }
    }

    if(not has_routed) {
        if(not _nodes.is_disconnected(message.target_id)) [[likely]] {
            const std::unique_lock lk{_router_lock};
            for(const auto& [outgoing_id, node_out] : _nodes.get()) {
                if(incoming_id != outgoing_id) {
                    has_routed |= node_out.try_route(*this, msg_id, message);
                }
            }
            // if the message didn't come from the parent router
            if(incoming_id != own_id) {
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

    const std::unique_lock lk{_router_lock};
    for(const auto& [outgoing_id, node_out] : _nodes.get()) {
        if(incoming_id != outgoing_id) {
            if(node_out.is_allowed(msg_id)) {
                _forward_to(node_out, msg_id, message);
            }
        }
    }
    if(not has_id(incoming_id)) {
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
    if(not message.too_many_hops()) [[likely]] {
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
        _stats.message_dropped();
    }
    return result;
}
//------------------------------------------------------------------------------
auto router::_handle_parent_message(
  const identifier_t incoming_id,
  const std::chrono::steady_clock::duration message_age_inc,
  const message_id msg_id,
  const message_age msg_age,
  message_view message) noexcept -> bool {
    _stats.update_avg_msg_age(message.add_age(msg_age).age() + message_age_inc);

    if(is_special_message(msg_id)) {
        return _handle_special_parent_message(msg_id, message);
    }
    if(message.too_old()) [[unlikely]] {
        _stats.message_dropped();
        return true;
    }
    return _route_message(msg_id, incoming_id, message);
}
//------------------------------------------------------------------------------
auto router::_handle_node_message(
  const identifier_t incoming_id,
  const std::chrono::steady_clock::duration message_age_inc,
  const message_id msg_id,
  const message_age msg_age,
  message_view message,
  routed_node& node) noexcept -> bool {
    _stats.update_avg_msg_age(message.add_age(msg_age).age() + message_age_inc);

    if(_handle_special(msg_id, incoming_id, node, message)) {
        return true;
    }
    if(message.too_old()) [[unlikely]] {
        _stats.message_dropped();
        return true;
    }
    return _route_message(msg_id, incoming_id, message);
}
//------------------------------------------------------------------------------
auto router::_handle_special_parent_message(
  const message_id msg_id,
  message_view& message) noexcept -> bool {
    switch(msg_id.method_id()) {
        case id_v("byeByeEndp"):
        case id_v("byeByeRutr"):
        case id_v("byeByeBrdg"):
            _parent_router.handle_bye(*this, msg_id, message);
            break;
        case id_v("confirmId"):
            _parent_router.confirm_id(*this, message);
            break;
        [[likely]] default:
            if(not _do_handle_special(msg_id, _parent_router.id(), message)) {
                return _route_message(msg_id, get_id(), message);
            }
    }
    return true;
}
//------------------------------------------------------------------------------
void router::_route_messages_by_workers(
  some_true_atomic& something_done) noexcept {
    const auto message_age_inc{_stats.time_since_last_routing()};
    std::latch completed{limit_cast<std::ptrdiff_t>(node_count())};

    for(auto& [node_id, node] : _nodes.get()) {
        node.enqueue_route_messages(
          workers(), *this, node_id, message_age_inc, completed, something_done);
    }

    something_done(_parent_router.route_messages(*this, message_age_inc));

    completed.wait();
}
//------------------------------------------------------------------------------
auto router::_route_messages_by_router() noexcept -> work_done {
    some_true something_done{};
    const auto message_age_inc{_stats.time_since_last_routing()};

    for(auto& [node_id, node] : _nodes.get()) {
        something_done(node.route_messages(*this, node_id, message_age_inc));
    }

    something_done(_parent_router.route_messages(*this, message_age_inc));

    return something_done;
}
//------------------------------------------------------------------------------
void router::_update_connections_by_workers(
  some_true_atomic& something_done) noexcept {
    std::latch completed{limit_cast<std::ptrdiff_t>(node_count())};

    for(auto& entry : _nodes.get()) {
        std::get<1>(entry).enqueue_update_connection(
          workers(), completed, something_done);
    }
    something_done(_parent_router.update(*this, get_id()));

    if(_nodes.has_some()) [[likely]] {
        _no_connection_timeout.reset();
    }

    completed.wait();
}
//------------------------------------------------------------------------------
auto router::_update_connections_by_router() noexcept -> work_done {
    some_true something_done{};

    for(auto& entry : _nodes.get()) {
        std::get<1>(entry).update_connection();
    }
    something_done(_parent_router.update(*this, get_id()));

    if(_nodes.has_some()) [[likely]] {
        _no_connection_timeout.reset();
    }
    return something_done;
}
//------------------------------------------------------------------------------
auto router::do_maintenance() noexcept -> work_done {
    some_true something_done{};

    something_done(_update_stats());
    something_done(_process_blobs());
    something_done(_nodes.handle_pending(*this));
    something_done(_nodes.handle_accept(*this));
    something_done(_nodes.remove_timeouted(*this));
    something_done(_nodes.remove_disconnected(*this));

    return something_done;
}
//------------------------------------------------------------------------------
auto router::do_work_by_workers() noexcept -> work_done {
    some_true_atomic something_done{};

    _route_messages_by_workers(something_done);
    _update_connections_by_workers(something_done);

    return something_done;
}
//------------------------------------------------------------------------------
auto router::do_work_by_router() noexcept -> work_done {
    some_true something_done{};

    something_done(_route_messages_by_router());
    something_done(_update_connections_by_router());

    return something_done;
}
//------------------------------------------------------------------------------
auto router::do_work() noexcept -> work_done {
    if(_use_workers()) {
        return do_work_by_workers();
    } else {
        return do_work_by_router();
    }
}
//------------------------------------------------------------------------------
auto router::update(const valid_if_positive<int>& count) noexcept -> work_done {
    const auto exec_time{measure_time_interval("busUpdate")};
    some_true something_done{};

    something_done(do_maintenance());

    int n = count.value_or(2);
    if(_use_workers()) {
        do {
            something_done(do_work_by_workers());
        } while((n-- > 0) and something_done);
    } else {
        do {
            something_done(do_work_by_router());
        } while((n-- > 0) and something_done);
    }

    return something_done;
}
//------------------------------------------------------------------------------
void router::say_bye() noexcept {
    const auto msgid{msgbus_id{"byeByeRutr"}};
    message_view msg{};
    msg.set_source_id(get_id());
    for(auto& entry : _nodes.get()) {
        auto& node{std::get<1>(entry)};
        node.send(*this, msgid, msg);
        node.update_connection();
    }
    _parent_router.send(*this, msgid, msg);
}
//------------------------------------------------------------------------------
void router::cleanup() noexcept {
    _nodes.cleanup();
    _stats.log_stats(*this);
}
//------------------------------------------------------------------------------
void router::finish() noexcept {
    say_bye();
    timeout too_long{adjusted_duration(std::chrono::seconds{1})};
    while(not too_long) {
        update(8);
    }
    cleanup();
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.core:router;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.container;
import eagine.core.utility;
import eagine.core.valid_if;
import eagine.core.runtime;
import eagine.core.main_ctx;
import :types;
import :message;
import :interface;
import :blobs;
import :context;

namespace eagine::msgbus {
export class router;
struct routed_node;
struct router_blobs;
//------------------------------------------------------------------------------
enum message_handling_result : bool {
    should_be_forwarded = false,
    was_handled = true
};
//------------------------------------------------------------------------------
class router_endpoint_info {
public:
    void add_subscription(const message_id) noexcept;
    void remove_subscription(const message_id) noexcept;
    auto is_subscribed_to(const message_id) noexcept -> bool;
    auto is_not_subscribed_to(const message_id) noexcept -> bool;
    auto subscriptions() noexcept -> std::vector<message_id>;

    auto has_instance_id() noexcept -> bool;
    auto instance_id() noexcept -> process_instance_id_t;
    void assign_instance_id(const message_view& msg) noexcept;
    void apply_instance_id(message_view& msg) noexcept;
    auto is_outdated() const noexcept -> bool;

private:
    std::vector<message_id> _subscriptions{};
    std::vector<message_id> _unsubscriptions{};
    process_instance_id_t _instance_id{0};
    timeout _is_outdated{adjusted_duration(std::chrono::seconds{60})};
};
//------------------------------------------------------------------------------
class router_pending {
public:
    router_pending(router&, unique_holder<connection>) noexcept;

    auto assigned_id() const noexcept -> identifier_t;

    auto password_is_required() const noexcept -> bool;
    auto should_request_password() const noexcept -> bool;
    auto try_request_password() noexcept -> work_done;

    auto maybe_router() const noexcept -> bool;
    auto node_kind() const noexcept -> string_view;
    auto connection_kind() const noexcept;
    auto connection_type() const noexcept;

    auto has_timeouted() noexcept -> bool;
    auto should_be_removed() noexcept -> bool;
    auto can_be_adopted() noexcept -> bool;

    auto update() noexcept -> work_done;

    void send(const message_id msg_id, const message_view&) noexcept;
    auto release_connection() noexcept -> unique_holder<msgbus::connection>;

private:
    void _assign_id() noexcept;
    auto _handle_msg(
      message_id msg_id,
      message_age,
      const message_view& msg) noexcept -> bool;
    auto _msg_handler() noexcept;
    auto _handle_send(
      const message_id msg_id,
      const message_age,
      const message_view& message) noexcept -> bool;
    auto _send_handler() noexcept;

    std::reference_wrapper<router> _parent;
    identifier_t _id{invalid_endpoint_id()};
    timeout _too_old{adjusted_duration(std::chrono::seconds{30})};
    timeout _should_request_pwd{
      adjusted_duration(std::chrono::seconds{3}),
      nothing};
    unique_holder<msgbus::connection> _connection{};
    std::vector<byte> _nonce{};
    identifier _connection_type;
    msgbus::connection_kind _connection_kind;
    bool _password_verified{false};
    bool _maybe_router{true};
};
//------------------------------------------------------------------------------
class connection_update_work_unit : public latched_work_unit {
public:
    connection_update_work_unit() noexcept = default;
    connection_update_work_unit(
      routed_node& node,
      std::latch& completed,
      some_true_atomic& something_done) noexcept
      : latched_work_unit{completed}
      , _node{&node}
      , _something_done{&something_done} {}

    auto do_it() noexcept -> bool final;

private:
    routed_node* _node{nullptr};
    some_true_atomic* _something_done{nullptr};
};
//------------------------------------------------------------------------------
class routed_node {
public:
    routed_node() noexcept;
    routed_node(routed_node&&) noexcept = default;
    routed_node(const routed_node&) = delete;
    auto operator=(routed_node&&) noexcept -> routed_node& = default;
    auto operator=(const routed_node&) -> routed_node& = delete;
    ~routed_node() noexcept = default;

    void block_message(const message_id) noexcept;
    void allow_message(const message_id) noexcept;
    void clear_block_list() noexcept;
    void clear_allow_list() noexcept;

    auto is_allowed(const message_id) const noexcept -> bool;

    void setup(unique_holder<connection>, bool maybe_router) noexcept;

    void enqueue_update_connection(
      workshop&,
      std::latch&,
      some_true_atomic&) noexcept;

    void mark_not_a_router() noexcept;
    auto do_update_connection() noexcept -> work_done;
    auto update_connection() noexcept -> work_done;
    void handle_bye_bye() noexcept;
    auto should_disconnect() const noexcept -> bool;
    void cleanup_connection() noexcept;
    auto kind_of_connection() const noexcept -> connection_kind;
    auto query_statistics(connection_statistics&) const noexcept -> bool;

    auto send(const main_ctx_object&, const message_id, const message_view&)
      const noexcept -> bool;

    auto route_messages(
      router&,
      const identifier_t incoming_id,
      const std::chrono::steady_clock::duration message_age_inc) noexcept
      -> work_done;

    auto try_route(
      const main_ctx_object&,
      const message_id,
      const message_view&) const noexcept -> bool;

    auto process_blobs(const identifier_t node_id, router_blobs& blobs) noexcept
      -> work_done;

private:
    unique_holder<std::shared_mutex> _list_lock{default_selector};
    unique_holder<connection> _connection{};
    connection_update_work_unit _update_connection_work{};
    std::vector<message_id> _message_block_list{};
    std::vector<message_id> _message_allow_list{};
    bool _maybe_router{true};
    bool _do_disconnect{false};
};
//------------------------------------------------------------------------------
class parent_router {
public:
    void reset(unique_holder<connection>) noexcept;

    explicit operator bool() const noexcept {
        return _connection and _confirmed_id;
    }

    auto id() const noexcept -> identifier_t {
        return _confirmed_id;
    }

    void confirm_id(
      const main_ctx_object&,
      const message_view& message) noexcept;

    void handle_bye(const main_ctx_object&, message_id, const message_view&)
      const noexcept;

    void announce_id(main_ctx_object&, const identifier_t id_base) noexcept;
    auto query_statistics(connection_statistics&) const noexcept -> bool;

    auto kind_of_connection() const noexcept -> connection_kind;

    auto update(main_ctx_object&, const identifier_t id_base) noexcept
      -> work_done;

    auto send(const main_ctx_object&, const message_id, const message_view&)
      const noexcept -> bool;

    auto route_messages(
      router&,
      const std::chrono::steady_clock::duration message_age_inc) noexcept
      -> work_done;

private:
    unique_holder<connection> _connection{};
    identifier_t _confirmed_id{0};
    timeout _confirm_id_timeout{
      adjusted_duration(std::chrono::seconds{2}),
      nothing};
};
//------------------------------------------------------------------------------
class router_nodes {
public:
    auto get() noexcept;
    auto count() noexcept -> std::size_t;
    auto has_id(const identifier_t id) noexcept -> bool;
    auto find(const identifier_t id) noexcept
      -> optional_reference<routed_node>;
    auto find_outgoing(const identifier_t target_id) -> valid_endpoint_id;
    auto has_some() noexcept -> bool;
    void add_acceptor(std::shared_ptr<acceptor> an_acceptor) noexcept;
    auto handle_pending(router&) noexcept -> work_done;
    auto handle_accept(router&) noexcept -> work_done;
    auto remove_timeouted(const main_ctx_object&) noexcept -> work_done;
    auto is_disconnected(const identifier_t endpoint_id) const noexcept -> bool;
    void mark_disconnected(const identifier_t endpoint_id) noexcept;
    auto remove_disconnected(const main_ctx_object&) noexcept -> work_done;
    auto update_endpoint_info(
      const identifier_t incoming_id,
      const message_view& message) noexcept -> router_endpoint_info&;

    auto subscribes_to(const identifier_t, const message_id) noexcept
      -> std::tuple<tribool, tribool, process_instance_id_t>;
    auto subscriptions_of(const identifier_t target_id) noexcept
      -> std::tuple<std::vector<message_id>, process_instance_id_t>;
    void erase(const identifier_t) noexcept;
    void cleanup() noexcept;

private:
    void _adopt_pending(router&, router_pending&) noexcept;
    auto _do_handle_pending(router&) noexcept -> work_done;

    small_vector<std::shared_ptr<acceptor>, 2> _acceptors;
    std::vector<router_pending> _pending;
    flat_map<identifier_t, routed_node> _nodes;
    flat_map<identifier_t, identifier_t> _endpoint_idx;
    flat_map<identifier_t, router_endpoint_info> _endpoint_infos;
    flat_map<identifier_t, timeout> _recently_disconnected;
};
//------------------------------------------------------------------------------
class router_stats {
public:
    auto uptime() noexcept -> std::chrono::seconds;
    auto time_since_last_routing() noexcept -> auto;
    auto update_stats() noexcept
      -> std::tuple<std::optional<message_flow_info>>;
    void update_avg_msg_age(
      const std::chrono::steady_clock::duration message_age_inc) noexcept;
    auto avg_msg_age() noexcept -> std::chrono::microseconds;
    auto statistics() noexcept -> router_statistics;

    void message_dropped() noexcept;
    void log_stats(const main_ctx_object&) noexcept;

private:
    using lockable = std::mutex;

    lockable _main_lock;
    std::chrono::steady_clock::time_point _startup_time{
      std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point _prev_route_time{
      std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point _forwarded_since_log{
      std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point _forwarded_since_stat{
      std::chrono::steady_clock::now()};
    basic_sliding_average<std::chrono::steady_clock::duration, std::int32_t, 8, 64>
      _message_age_avg{};
    std::int64_t _prev_forwarded_messages{0};
    router_statistics _stats{};
    message_flow_info _flow_info{};
};
//------------------------------------------------------------------------------
class router_ids {
public:
    auto router_id() noexcept -> identifier_t;
    auto instance_id() noexcept -> process_instance_id_t;
    void set_description(main_ctx_object&);
    void setup_from_config(const main_ctx_object&);

    auto get_next_id(router&) noexcept -> identifier_t;

private:
    identifier_t _id_base{0};
    identifier_t _id_end{0};
    identifier_t _id_sequence{0};
    const process_instance_id_t _instance_id{process_instance_id()};
};
//------------------------------------------------------------------------------
class router_context {
public:
    router_context(shared_context) noexcept;
    auto add_certificate_pem(const memory::const_block blk) noexcept -> bool;
    auto add_ca_certificate_pem(const memory::const_block blk) noexcept -> bool;
    auto add_remote_certificate_pem(
      const identifier_t,
      const memory::const_block blk) noexcept -> bool;
    auto get_own_certificate_pem() noexcept -> memory::const_block;
    auto get_remote_certificate_pem(const identifier_t) noexcept
      -> memory::const_block;

private:
    using lockable = std::mutex;

    lockable _context_lock;
    shared_context _context{};
};
//------------------------------------------------------------------------------
class router_blobs {
public:
    using send_handler = blob_manipulator::send_handler;
    using fetch_handler = blob_manipulator::fetch_handler;

    router_blobs(router& parent) noexcept;

    auto has_outgoing() noexcept -> bool;

    auto process_blobs(const identifier_t parent_id, router& parent) noexcept
      -> work_done;

    auto process_outgoing(
      const send_handler,
      const span_size_t max_data_size,
      span_size_t max_messages) noexcept -> work_done;

    void push_outgoing(
      const message_id msg_id,
      const identifier_t source_id,
      const identifier_t target_id,
      const blob_id_t target_blob_id,
      const memory::const_block blob,
      const std::chrono::seconds max_time,
      const message_priority priority) noexcept;

    void handle_fragment(
      const message_view& message,
      const fetch_handler) noexcept;

    void handle_resend(const message_view& message) noexcept;

private:
    auto _get_blob_target_io(
      const message_id,
      const span_size_t,
      blob_manipulator&) noexcept -> std::unique_ptr<target_blob_io>;

    blob_manipulator _blobs;
};
//------------------------------------------------------------------------------
export class router
  : public main_ctx_object
  , public acceptor_user
  , public connection_user {
public:
    router(main_ctx_parent parent) noexcept;

    /// @brief Returns the unique id of this router.
    auto get_id() noexcept -> identifier_t;

    auto has_id(const identifier_t) noexcept -> bool;
    auto has_node_id(const identifier_t) noexcept -> bool;
    auto password_is_required() const noexcept -> bool;

    void add_certificate_pem(const memory::const_block blk) noexcept;
    void add_ca_certificate_pem(const memory::const_block blk) noexcept;

    auto add_acceptor(std::shared_ptr<acceptor>) noexcept -> bool final;
    auto add_connection(std::unique_ptr<connection>) noexcept -> bool final;

    auto do_maintenance() noexcept -> work_done;
    auto do_work_by_workers() noexcept -> work_done;
    auto do_work_by_router() noexcept -> work_done;
    auto do_work() noexcept -> work_done;

    auto update(const valid_if_positive<int>& count) noexcept -> work_done;
    auto update() noexcept -> work_done {
        return update(2);
    }

    void say_bye() noexcept;
    void cleanup() noexcept;
    void finish() noexcept;

    auto no_connection_timeout() const noexcept -> auto& {
        return _no_connection_timeout;
    }

    auto is_done() const noexcept -> bool {
        return no_connection_timeout().is_expired();
    }

private:
    friend class parent_router;
    friend class routed_node;
    friend class router_pending;
    friend class router_nodes;
    friend class router_blobs;

    auto _uptime_seconds() noexcept -> std::int64_t;
    auto _remove_disconnected() noexcept -> work_done;

    auto _get_next_id() noexcept -> identifier_t;

    auto _current_nodes() noexcept;

    auto _process_blobs() noexcept -> work_done;

    auto _handle_blob(
      const message_id,
      const message_age,
      const message_view&) noexcept -> bool;

    auto _update_endpoint_info(
      const identifier_t incoming_id,
      const message_view&) noexcept -> router_endpoint_info&;

    auto _send_flow_info(const message_flow_info&) noexcept -> work_done;

    auto _handle_ping(const message_view&) noexcept -> message_handling_result;

    auto _handle_subscribed(
      const identifier_t incoming_id,
      const message_view&) noexcept -> message_handling_result;

    auto _handle_clear_block_list(routed_node& node) noexcept
      -> message_handling_result;
    auto _handle_clear_allow_list(routed_node& node) noexcept
      -> message_handling_result;
    auto _handle_still_alive(
      const identifier_t incoming_id,
      const message_view&) noexcept -> message_handling_result;
    auto _handle_not_not_a_router(
      const identifier_t incoming_id,
      routed_node& node,
      const message_view&) noexcept -> message_handling_result;
    auto _handle_not_subscribed(
      const identifier_t incoming_id,
      const message_view&) noexcept -> message_handling_result;
    auto _handle_msg_allow(
      const identifier_t incoming_id,
      routed_node& node,
      const message_view&) noexcept -> message_handling_result;
    auto _handle_msg_block(
      const identifier_t incoming_id,
      routed_node& node,
      const message_view&) noexcept -> message_handling_result;

    auto _handle_subscribers_query(const message_view&) noexcept
      -> message_handling_result;

    auto _handle_subscriptions_query(const message_view&) noexcept
      -> message_handling_result;

    auto _handle_password_request(const message_view&) noexcept
      -> message_handling_result;

    auto _handle_router_certificate_query(const message_view&) noexcept
      -> message_handling_result;
    auto _handle_endpoint_certificate_query(const message_view&) noexcept
      -> message_handling_result;

    auto _handle_topology_query(const message_view&) noexcept
      -> message_handling_result;

    auto _avg_msg_age() const noexcept -> std::chrono::microseconds;
    auto _update_stats() noexcept -> work_done;
    auto _handle_stats_query(const message_view&) noexcept
      -> message_handling_result;

    auto _handle_bye_bye(
      const message_id,
      routed_node& node,
      const message_view&) noexcept -> message_handling_result;

    auto _handle_blob_fragment(const message_view&) noexcept
      -> message_handling_result;
    auto _handle_blob_resend(const message_view&) noexcept
      -> message_handling_result;

    auto _handle_special_common(
      const message_id msg_id,
      const identifier_t incoming_id,
      const message_view&) noexcept -> message_handling_result;

    auto _do_handle_special(
      const message_id msg_id,
      const identifier_t incoming_id,
      const message_view&) noexcept -> message_handling_result;

    auto _do_handle_special(
      const message_id msg_id,
      const identifier_t incoming_id,
      routed_node&,
      const message_view&) noexcept -> message_handling_result;

    auto _handle_special(
      const message_id msg_id,
      const identifier_t incoming_id,
      const message_view&) noexcept -> message_handling_result;

    auto _handle_special(
      const message_id msg_id,
      const identifier_t incoming_id,
      routed_node&,
      const message_view&) noexcept -> message_handling_result;

    auto _use_workers() const noexcept -> bool {
        return _use_worker_threads;
    }
    void _update_use_workers() noexcept;

    auto _forward_to(
      const routed_node& node_out,
      const message_id msg_id,
      message_view& message) noexcept -> bool;
    auto _route_targeted_message(
      const message_id msg_id,
      const identifier_t incoming_id,
      message_view& message) noexcept -> bool;
    auto _route_broadcast_message(
      const message_id msg_id,
      const identifier_t incoming_id,
      message_view& message) noexcept -> bool;
    auto _route_message(
      const message_id msg_id,
      const identifier_t incoming_id,
      message_view& message) noexcept -> bool;
    auto _handle_parent_message(
      const identifier_t incoming_id,
      const std::chrono::steady_clock::duration message_age_inc,
      const message_id msg_id,
      const message_age msg_age,
      message_view message) noexcept -> bool;
    auto _handle_node_message(
      const identifier_t incoming_id,
      const std::chrono::steady_clock::duration message_age_inc,
      const message_id msg_id,
      const message_age msg_age,
      message_view message,
      routed_node&) noexcept -> bool;

    auto _handle_special_parent_message(
      const message_id msg_id,
      message_view& message) noexcept -> bool;
    void _route_messages_by_workers(some_true_atomic&) noexcept;
    auto _route_messages_by_router() noexcept -> work_done;
    void _update_connections_by_workers(some_true_atomic&) noexcept;
    auto _update_connections_by_router() noexcept -> work_done;

    using lockable = std::mutex;

    router_context _context;
    router_ids _ids;
    router_stats _stats;
    parent_router _parent_router;
    router_nodes _nodes;
    router_blobs _blobs{*this};

    timeout _no_connection_timeout{adjusted_duration(std::chrono::seconds{30})};

    bool _password_is_required{false};
    bool _use_worker_threads{false};
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


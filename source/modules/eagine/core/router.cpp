/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.core:router;

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
import std;

namespace eagine::msgbus {
export class router;
struct routed_node;
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
    auto instance_id(const message_view& msg) noexcept -> process_instance_id_t;
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
    router_pending(std::unique_ptr<connection> a_connection) noexcept
      : _connection{std::move(a_connection)} {}

    auto age() const noexcept;

    auto update_connection() noexcept -> work_done;

    auto connection() noexcept -> msgbus::connection&;

    auto release_connection() noexcept -> std::unique_ptr<msgbus::connection>;

private:
    std::chrono::steady_clock::time_point _create_time{
      std::chrono::steady_clock::now()};

    std::unique_ptr<msgbus::connection> _connection{};
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

    void setup(std::unique_ptr<connection>, bool maybe_router) noexcept;

    void enqueue_update_connection(
      workshop&,
      std::latch&,
      some_true_atomic&) noexcept;

    void mark_not_a_router() noexcept;
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

    auto process_blobs(
      const identifier_t node_id,
      blob_manipulator& blobs) noexcept -> work_done;

private:
    using lockable = std::mutex;

    std::unique_ptr<lockable> _list_lock{std::make_unique<lockable>()};
    std::vector<message_id> _message_block_list{};
    std::vector<message_id> _message_allow_list{};
    std::unique_ptr<connection> _connection{};
    connection_update_work_unit _update_connection_work{};
    bool _maybe_router{true};
    bool _do_disconnect{false};
};
//------------------------------------------------------------------------------
class parent_router {
public:
    void reset(std::unique_ptr<connection>) noexcept;

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
    std::unique_ptr<connection> _connection{};
    identifier_t _confirmed_id{0};
    timeout _confirm_id_timeout{
      adjusted_duration(std::chrono::seconds{2}),
      nothing};
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

    void add_certificate_pem(const memory::const_block blk) noexcept;
    void add_ca_certificate_pem(const memory::const_block blk) noexcept;

    auto add_acceptor(std::shared_ptr<acceptor>) noexcept -> bool final;
    auto add_connection(std::unique_ptr<connection>) noexcept -> bool final;

    auto do_maintenance() noexcept -> work_done;
    auto do_work_by_workers() noexcept -> work_done;
    auto do_work_by_router() noexcept -> work_done;
    auto do_work() noexcept -> work_done {
        if(_use_workers()) {
            return do_work_by_workers();
        } else {
            return do_work_by_router();
        }
    }

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

    void post_blob(
      const message_id msg_id,
      const identifier_t source_id,
      const identifier_t target_id,
      const blob_id_t target_blob_id,
      const memory::const_block blob,
      const std::chrono::seconds max_time,
      const message_priority priority) noexcept;

private:
    friend class routed_node;
    friend class parent_router;

    auto _uptime_seconds() noexcept -> std::int64_t;

    auto _handle_accept() noexcept -> work_done;
    auto _do_handle_pending() noexcept -> work_done;
    auto _handle_pending() noexcept -> work_done;
    auto _remove_timeouted() noexcept -> work_done;
    auto _is_disconnected(const identifier_t) const noexcept -> bool;
    auto _mark_disconnected(const identifier_t endpoint_id) noexcept -> void;
    auto _remove_disconnected() noexcept -> work_done;
    void _assign_id(connection& conn) noexcept;
    void _handle_connection(std::unique_ptr<connection> conn) noexcept;

    auto _process_blobs() noexcept -> work_done;
    auto _do_get_blob_target_io(
      const message_id,
      const span_size_t,
      blob_manipulator&) noexcept -> std::unique_ptr<target_blob_io>;

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

    const std::chrono::seconds _pending_timeout{
      adjusted_duration(std::chrono::seconds{30})};
    timeout _no_connection_timeout{adjusted_duration(std::chrono::seconds{30})};

    router_stats _stats;

    parent_router _parent_router;
    small_vector<std::shared_ptr<acceptor>, 2> _acceptors;
    std::vector<router_pending> _pending;
    flat_map<identifier_t, routed_node> _nodes;
    flat_map<identifier_t, identifier_t> _endpoint_idx;
    flat_map<identifier_t, router_endpoint_info> _endpoint_infos;
    flat_map<identifier_t, timeout> _recently_disconnected;
    blob_manipulator _blobs{
      *this,
      msgbus_id{"blobFrgmnt"},
      msgbus_id{"blobResend"}};
    bool _use_worker_threads{false};
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


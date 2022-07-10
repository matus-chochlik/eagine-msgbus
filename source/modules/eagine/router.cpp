/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus:router;

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
import <map>;
import <vector>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
struct router_pending {

    router_pending(std::unique_ptr<connection> a_connection) noexcept
      : the_connection{std::move(a_connection)} {}

    std::chrono::steady_clock::time_point create_time{
      std::chrono::steady_clock::now()};

    std::unique_ptr<connection> the_connection{};

    auto age() const noexcept {
        return std::chrono::steady_clock::now() - create_time;
    }
};
//------------------------------------------------------------------------------
struct router_endpoint_info {
    process_instance_id_t instance_id{0};
    timeout is_outdated{adjusted_duration(std::chrono::seconds{60})};
    std::vector<message_id> subscriptions{};
    std::vector<message_id> unsubscriptions{};

    void assign_instance_id(const message_view& msg) noexcept {
        is_outdated.reset();
        if(instance_id != msg.sequence_no) {
            instance_id = msg.sequence_no;
            subscriptions.clear();
            unsubscriptions.clear();
        }
    }
};
//------------------------------------------------------------------------------
struct routed_node {
    std::unique_ptr<connection> the_connection{};
    std::vector<message_id> message_block_list{};
    std::vector<message_id> message_allow_list{};
    bool maybe_router{true};
    bool do_disconnect{false};

    routed_node() noexcept;
    routed_node(routed_node&&) noexcept = default;
    routed_node(const routed_node&) = delete;
    auto operator=(routed_node&&) noexcept -> routed_node& = default;
    auto operator=(const routed_node&) -> routed_node& = delete;
    ~routed_node() noexcept = default;

    void block_message(const message_id) noexcept;
    void allow_message(const message_id) noexcept;

    auto is_allowed(const message_id) const noexcept -> bool;

    auto send(main_ctx_object&, const message_id, const message_view&)
      const noexcept -> bool;
};
//------------------------------------------------------------------------------
struct parent_router {
    std::unique_ptr<connection> the_connection{};
    identifier_t confirmed_id{0};
    timeout confirm_id_timeout{
      adjusted_duration(std::chrono::seconds{2}),
      nothing};

    void reset(std::unique_ptr<connection>) noexcept;

    auto update(main_ctx_object&, const identifier_t id_base) noexcept
      -> work_done;

    template <typename Handler>
    auto fetch_messages(main_ctx_object&, const Handler&) noexcept -> work_done;

    auto send(main_ctx_object&, const message_id, const message_view&)
      const noexcept -> bool;
};
//------------------------------------------------------------------------------
export class router
  : public main_ctx_object
  , public acceptor_user
  , public connection_user {
public:
    router(main_ctx_parent parent) noexcept
      : main_ctx_object{identifier{"MsgBusRutr"}, parent}
      , _context{make_context(*this)} {
        _setup_from_config();

        using std::to_string;
        object_description(
          "Router-" + to_string(_id_base),
          "Message bus router id " + to_string(_id_base));
    }

    void add_certificate_pem(const memory::const_block blk) noexcept;
    void add_ca_certificate_pem(const memory::const_block blk) noexcept;

    auto add_acceptor(std::shared_ptr<acceptor>) noexcept -> bool final;
    auto add_connection(std::unique_ptr<connection>) noexcept -> bool final;

    auto do_maintenance() noexcept -> work_done;
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
        return bool(no_connection_timeout());
    }

    void post_blob(
      const message_id msg_id,
      const identifier_t source_id,
      const identifier_t target_id,
      const blob_id_t target_blob_id,
      const memory::const_block blob,
      const std::chrono::seconds max_time,
      const message_priority priority) noexcept {
        _blobs.push_outgoing(
          msg_id,
          source_id,
          target_id,
          target_blob_id,
          blob,
          max_time,
          priority);
    }

private:
    auto _uptime_seconds() noexcept -> std::int64_t;

    void _setup_from_config();

    auto _handle_accept() noexcept -> work_done;
    auto _handle_pending() noexcept -> work_done;
    auto _remove_timeouted() noexcept -> work_done;
    auto _is_disconnected(const identifier_t endpoint_id) noexcept -> bool;
    auto _mark_disconnected(const identifier_t endpoint_id) noexcept -> void;
    auto _remove_disconnected() noexcept -> work_done;
    void _assign_id(std::unique_ptr<connection>& conn) noexcept;
    void _handle_connection(std::unique_ptr<connection> conn) noexcept;

    auto _process_blobs() noexcept -> work_done;
    auto _do_get_blob_io(
      const message_id,
      const span_size_t,
      blob_manipulator&) noexcept -> std::unique_ptr<blob_io>;

    enum message_handling_result { should_be_forwarded, was_handled };

    auto _handle_blob(
      const message_id,
      const message_age,
      const message_view&) noexcept -> bool;

    auto _update_endpoint_info(
      const identifier_t incoming_id,
      const message_view&) noexcept -> router_endpoint_info&;

    auto _handle_ping(const message_view&) noexcept -> message_handling_result;

    auto _handle_subscribed(
      const identifier_t incoming_id,
      const message_view&) noexcept -> message_handling_result;

    auto _handle_not_subscribed(
      const identifier_t incoming_id,
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

    auto _update_stats() noexcept -> work_done;
    auto _handle_stats_query(const message_view&) noexcept
      -> message_handling_result;

    auto _handle_blob_fragment(const message_view&) noexcept
      -> message_handling_result;
    auto _handle_blob_resend(const message_view&) noexcept
      -> message_handling_result;

    auto _handle_special_common(
      const message_id msg_id,
      const identifier_t incoming_id,
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

    auto _do_route_message(
      const message_id msg_id,
      const identifier_t incoming_id,
      message_view& message) noexcept -> bool;

    auto _route_messages() noexcept -> work_done;
    auto _update_connections() noexcept -> work_done;

    shared_context _context{};
    const std::chrono::seconds _pending_timeout{
      adjusted_duration(std::chrono::seconds{30})};
    timeout _no_connection_timeout{adjusted_duration(std::chrono::seconds{30})};
    const process_instance_id_t _instance_id{process_instance_id()};
    identifier_t _id_base{0};
    identifier_t _id_end{0};
    identifier_t _id_sequence{0};
    std::chrono::steady_clock::time_point _startup_time{
      std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point _prev_route_time{
      std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point _forwarded_since_log{
      std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point _forwarded_since_stat{
      std::chrono::steady_clock::now()};
    std::int64_t _prev_forwarded_messages{0};
    float _message_age_sum{0.F};
    router_statistics _stats{};
    message_flow_info _flow_info{};

    parent_router _parent_router;
    std::vector<std::shared_ptr<acceptor>> _acceptors;
    std::vector<router_pending> _pending;
    flat_map<identifier_t, routed_node> _nodes;
    flat_map<identifier_t, identifier_t> _endpoint_idx;
    flat_map<identifier_t, router_endpoint_info> _endpoint_infos;
    flat_map<identifier_t, timeout> _recently_disconnected;
    blob_manipulator _blobs{
      *this,
      msgbus_id{"blobFrgmnt"},
      msgbus_id{"blobResend"}};
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


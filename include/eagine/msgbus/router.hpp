/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#ifndef EAGINE_MSGBUS_ROUTER_HPP
#define EAGINE_MSGBUS_ROUTER_HPP

#include "acceptor.hpp"
#include "blobs.hpp"
#include "context_fwd.hpp"
#include <eagine/bool_aggregate.hpp>
#include <eagine/flat_map.hpp>
#include <eagine/main_ctx_object.hpp>
#include <eagine/timeout.hpp>
#include <eagine/valid_if/positive.hpp>
#include <map>
#include <vector>

namespace eagine::msgbus {
//------------------------------------------------------------------------------
struct router_pending {

    router_pending(std::unique_ptr<connection> a_connection)
      : the_connection{std::move(a_connection)} {}

    std::chrono::steady_clock::time_point create_time{
      std::chrono::steady_clock::now()};

    std::unique_ptr<connection> the_connection{};

    auto age() const {
        return std::chrono::steady_clock::now() - create_time;
    }
};
//------------------------------------------------------------------------------
struct router_endpoint_info {
    process_instance_id_t instance_id{0};
    timeout is_outdated{adjusted_duration(std::chrono::seconds{60})};
    std::vector<message_id> subscriptions{};
    std::vector<message_id> unsubscriptions{};

    void assign_instance_id(const message_view& msg) {
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

    routed_node();
    routed_node(routed_node&&) noexcept = default;
    routed_node(const routed_node&) = delete;
    auto operator=(routed_node&&) noexcept -> routed_node& = default;
    auto operator=(const routed_node&) -> routed_node& = delete;
    ~routed_node() noexcept = default;

    void block_message(const message_id);
    void allow_message(const message_id);

    auto is_allowed(const message_id) const noexcept -> bool;

    auto send(main_ctx_object&, const message_id, const message_view&) const
      -> bool;
};
//------------------------------------------------------------------------------
struct parent_router {
    std::unique_ptr<connection> the_connection{};
    identifier_t confirmed_id{0};
    timeout confirm_id_timeout{
      adjusted_duration(std::chrono::seconds{2}),
      nothing};

    void reset(std::unique_ptr<connection>);

    auto update(main_ctx_object&, identifier_t id_base) -> work_done;

    template <typename Handler>
    auto fetch_messages(main_ctx_object&, const Handler&) -> work_done;

    auto send(main_ctx_object&, message_id, const message_view&) const -> bool;
};
//------------------------------------------------------------------------------
class router
  : public main_ctx_object
  , public acceptor_user
  , public connection_user {
public:
    router(main_ctx_parent parent) noexcept
      : main_ctx_object{EAGINE_ID(MsgBusRutr), parent}
      , _context{make_context(*this)} {
        _setup_from_config();

        using std::to_string;
        object_description(
          "Router-" + to_string(_id_base),
          "Message bus router id " + to_string(_id_base));
    }

    void add_certificate_pem(memory::const_block blk);
    void add_ca_certificate_pem(memory::const_block blk);

    auto add_acceptor(std::shared_ptr<acceptor>) -> bool final;
    auto add_connection(std::unique_ptr<connection>) -> bool final;

    auto do_maintenance() -> work_done;
    auto do_work() -> work_done;

    auto update(const valid_if_positive<int>& count) -> work_done;
    auto update() -> work_done {
        return update(2);
    }

    void say_bye();
    void cleanup();
    void finish();

    auto no_connection_timeout() const noexcept -> auto& {
        return _no_connection_timeout;
    }

    auto is_done() const noexcept -> bool {
        return bool(no_connection_timeout());
    }

    void post_blob(
      message_id msg_id,
      identifier_t source_id,
      identifier_t target_id,
      blob_id_t target_blob_id,
      memory::const_block blob,
      std::chrono::seconds max_time,
      message_priority priority) {
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
    auto _uptime_seconds() -> std::int64_t;

    void _setup_from_config();

    auto _handle_accept() -> work_done;
    auto _handle_pending() -> work_done;
    auto _remove_timeouted() -> work_done;
    auto _is_disconnected(identifier_t endpoint_id) -> bool;
    auto _mark_disconnected(identifier_t endpoint_id) -> void;
    auto _remove_disconnected() -> work_done;
    void _assign_id(std::unique_ptr<connection>& conn);
    void _handle_connection(std::unique_ptr<connection> conn);

    auto _process_blobs() -> work_done;
    auto _do_get_blob_io(message_id, span_size_t, blob_manipulator&)
      -> std::unique_ptr<blob_io>;

    enum message_handling_result { should_be_forwarded, was_handled };

    auto _handle_blob(message_id, message_age, const message_view&) -> bool;

    auto _update_endpoint_info(identifier_t incoming_id, const message_view&)
      -> router_endpoint_info&;

    auto _handle_ping(const message_view&) -> message_handling_result;

    auto _handle_subscribed(identifier_t incoming_id, const message_view&)
      -> message_handling_result;

    auto _handle_not_subscribed(identifier_t incoming_id, const message_view&)
      -> message_handling_result;

    auto _handle_subscribers_query(const message_view&)
      -> message_handling_result;

    auto _handle_subscriptions_query(const message_view&)
      -> message_handling_result;

    auto _handle_router_certificate_query(const message_view&)
      -> message_handling_result;
    auto _handle_endpoint_certificate_query(const message_view&)
      -> message_handling_result;

    auto _handle_topology_query(const message_view&) -> message_handling_result;

    auto _update_stats() -> work_done;
    auto _handle_stats_query(const message_view&) -> message_handling_result;

    auto _handle_blob_fragment(const message_view&) -> message_handling_result;
    auto _handle_blob_resend(const message_view&) -> message_handling_result;

    auto _handle_special_common(
      message_id msg_id,
      identifier_t incoming_id,
      const message_view&) -> message_handling_result;

    auto _handle_special(
      message_id msg_id,
      identifier_t incoming_id,
      const message_view&) -> message_handling_result;

    auto _handle_special(
      message_id msg_id,
      identifier_t incoming_id,
      routed_node&,
      const message_view&) -> message_handling_result;

    auto _do_route_message(
      message_id msg_id,
      identifier_t incoming_id,
      message_view& message) -> bool;

    auto _route_messages() -> work_done;
    auto _update_connections() -> work_done;

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
      EAGINE_MSGBUS_ID(blobFrgmnt),
      EAGINE_MSGBUS_ID(blobResend)};
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

#if !EAGINE_MSGBUS_LIBRARY || defined(EAGINE_IMPLEMENTING_LIBRARY)
#include <eagine/msgbus/router.inl>
#endif

#endif // EAGINE_MSGBUS_ROUTER_HPP

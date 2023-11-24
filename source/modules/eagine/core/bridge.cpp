/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.core:bridge;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.utility;
import eagine.core.valid_if;
import eagine.core.main_ctx;
import :types;
import :message;
import :interface;
import :context;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
export class bridge_state;
export class bridge
  : public main_ctx_object
  , public connection_user {

public:
    bridge(main_ctx_parent parent) noexcept;

    void add_certificate_pem(const memory::const_block blk) noexcept;
    void add_ca_certificate_pem(const memory::const_block blk) noexcept;

    auto add_connection(unique_holder<connection>) noexcept -> bool final;

    auto has_id() const noexcept -> bool {
        return is_valid_endpoint_id(_id);
    }

    auto update() noexcept -> work_done;
    auto is_done() const noexcept -> bool;
    void say_bye() noexcept;
    void cleanup() noexcept;
    void finish() noexcept;

    auto no_connection_timeout() const noexcept -> auto& {
        return _no_connection_timeout;
    }

private:
    auto _uptime_seconds() noexcept -> std::int64_t;
    void _setup_from_config();

    auto _recoverable_state() const noexcept -> bool;
    auto _check_state() noexcept -> work_done;
    auto _update_connections() noexcept -> work_done;

    auto _do_send(const message_id, message_view&) noexcept -> bool;
    auto _send(const message_id, message_view&) noexcept -> bool;

    enum message_handling_result { should_be_forwarded, was_handled };

    auto _handle_id_assigned(const message_view&) noexcept
      -> message_handling_result;
    auto _handle_id_confirmed(const message_view&) noexcept
      -> message_handling_result;
    auto _handle_ping(const message_view&, const bool) noexcept
      -> message_handling_result;

    auto _handle_topo_bridge_conn(const message_view&, const bool) noexcept
      -> message_handling_result;
    auto _handle_topology_query(const message_view&, const bool) noexcept
      -> message_handling_result;
    auto _handle_stats_query(const message_view&, const bool) noexcept
      -> message_handling_result;

    auto _handle_special(
      const message_id,
      const message_view&,
      const bool) noexcept -> message_handling_result;

    auto _do_push(const message_id, message_view&) noexcept -> bool;
    auto _avg_msg_age_c2o() const noexcept -> std::chrono::microseconds;
    auto _avg_msg_age_i2c() const noexcept -> std::chrono::microseconds;
    auto _should_log_bridge_stats_c2o() noexcept -> bool;
    auto _should_log_bridge_stats_i2c() noexcept -> bool;
    void _log_bridge_stats_c2o() noexcept;
    void _log_bridge_stats_i2c() noexcept;
    auto _forward_messages() noexcept -> work_done;

    shared_context _context{};

    const process_instance_id_t _instance_id{process_instance_id()};
    endpoint_id_t _id{};
    timeout _no_id_timeout{adjusted_duration(std::chrono::seconds{2}), nothing};

    std::chrono::steady_clock::time_point _startup_time{
      std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point _forwarded_since_i2c{
      std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point _forwarded_since_c2o{
      std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point _forwarded_since_stat{
      std::chrono::steady_clock::now()};
    std::chrono::steady_clock::duration _message_age_sum_i2c;
    std::chrono::steady_clock::duration _message_age_sum_c2o;
    std::int64_t _state_count{0};
    std::int64_t _forwarded_messages_i2c{0};
    std::int64_t _forwarded_messages_c2o{0};
    std::int64_t _prev_forwarded_messages{0};
    std::int64_t _dropped_messages_i2c{0};
    std::int64_t _dropped_messages_c2o{0};
    bridge_statistics _stats{};

    shared_holder<bridge_state> _state{};
    timeout _no_connection_timeout{adjusted_duration(std::chrono::seconds{30})};
    unique_holder<connection> _connection{};
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


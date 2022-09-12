/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.utility:resource_transfer;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.valid_if;
import eagine.core.utility;
import eagine.core.runtime;
import eagine.core.main_ctx;
import eagine.msgbus.core;
import eagine.msgbus.services;
import <chrono>;
import <map>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
export using resource_data_server_node_base =
  service_composition<require_services<
    subscriber,
    shutdown_target,
    resource_server,
    pingable,
    common_info_providers>>;
//------------------------------------------------------------------------------
export class resource_data_server_node
  : public main_ctx_object
  , public resource_data_server_node_base {
    using base = resource_data_server_node_base;

    void _init();

public:
    resource_data_server_node(endpoint& bus)
      : main_ctx_object{"RsrcServer", bus}
      , base{bus} {
        _init();
    }

    resource_data_server_node(endpoint& bus, resource_server_driver& drvr)
      : main_ctx_object{"RsrcServer", bus}
      , base{bus, drvr} {
        _init();
    }

    auto is_done() const noexcept -> bool {
        return _done;
    }

private:
    void _handle_shutdown(
      const std::chrono::milliseconds age,
      const identifier_t source_id,
      const verification_bits verified) noexcept;

    bool _done{false};
};
//------------------------------------------------------------------------------
export using resource_data_consumer_node_base =
  service_node<require_services<subscriber, resource_manipulator, pinger>>;
//------------------------------------------------------------------------------
export class resource_data_consumer_node
  : public resource_data_consumer_node_base
  , public blob_stream_signals {
    using base = resource_data_consumer_node_base;

    void _init();

public:
    resource_data_consumer_node(main_ctx& ctx)
      : base{"RsrcCnsmer", ctx} {
        _init();
    }

    auto update() noexcept -> work_done;

    void query_resource(
      url locator,
      std::shared_ptr<blob_io> io,
      const message_priority priority,
      const std::chrono::seconds max_time) {
        _query_resource(
          _get_resource_id(),
          std::move(locator),
          std::move(io),
          priority,
          max_time);
    }

    auto stream_resource(
      url locator,
      const message_priority priority,
      const std::chrono::seconds max_time)
      -> std::pair<identifier_t, const url&>;

    auto cancel_resource_stream(identifier_t resource_id) noexcept -> bool;

    auto has_pending_resources() const noexcept -> bool {
        return !_pending_resources.empty();
    }

private:
    struct _server_info {
        timeout should_check{std::chrono::seconds{5}};
        timeout not_responding{std::chrono::seconds{10}};
    };

    struct _resource_info {
        url locator{};
        std::shared_ptr<blob_io> resource_io{};
        identifier_t source_server_id{invalid_endpoint_id()};
        timeout should_search{std::chrono::seconds{3}, nothing};
        timeout blob_timeout{};
        message_sequence_t blob_stream_id{0};
        message_priority blob_priority{message_priority::normal};
    };

    auto _has_pending(identifier_t) const noexcept -> bool;
    auto _get_resource_id() noexcept -> identifier_t;
    auto _query_resource(
      identifier_t res_id,
      url locator,
      std::shared_ptr<blob_io> io,
      const message_priority priority,
      const std::chrono::seconds max_time)
      -> std::pair<identifier_t, resource_data_consumer_node::_resource_info&>;

    void _handle_server_appeared(identifier_t) noexcept;
    void _handle_server_lost(identifier_t) noexcept;
    void _handle_resource_found(identifier_t, const url&) noexcept;
    void _handle_missing(identifier_t, const url&) noexcept;
    void _handle_stream_done(identifier_t) noexcept;
    void _handle_ping_response(
      const identifier_t pinger_id,
      const message_sequence_t,
      const std::chrono::microseconds age,
      const verification_bits) noexcept;
    void _handle_ping_timeout(
      const identifier_t pinger_id,
      const message_sequence_t,
      const std::chrono::microseconds) noexcept;

    identifier_t _res_id_seq{0};
    memory::buffer_pool _buffers;

    std::map<identifier_t, _server_info> _current_servers;
    std::map<identifier_t, _resource_info> _pending_resources;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

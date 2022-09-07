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
import eagine.core.container;
import eagine.core.valid_if;
import eagine.core.runtime;
import eagine.core.main_ctx;
import eagine.msgbus.core;
import eagine.msgbus.services;
import <chrono>;
import <filesystem>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
export using resource_data_server_node_base =
  service_composition<require_services<
    subscriber,
    shutdown_target,
    resource_server,
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
  service_composition<require_services<subscriber, resource_manipulator>>;
//------------------------------------------------------------------------------
export class resource_data_consumer_node
  : public main_ctx_object
  , public resource_data_consumer_node_base {
    using base = resource_data_consumer_node_base;

    void _init();

public:
    resource_data_consumer_node(endpoint& bus)
      : main_ctx_object{"RsrcCnsmer", bus}
      , base{bus} {
        _init();
    }

private:
    void _handle_server_appeared(identifier_t) noexcept;
    void _handle_server_lost(identifier_t) noexcept;

    flat_set<identifier_t> _server_ids;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

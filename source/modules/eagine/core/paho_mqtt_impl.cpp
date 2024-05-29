/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
module;

module eagine.msgbus.core;

import std;
import eagine.core.debug;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.string;
import eagine.core.identifier;
import eagine.core.utility;
import eagine.core.valid_if;
import eagine.core.main_ctx;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
// Shared state
//------------------------------------------------------------------------------
struct paho_mqtt_common_state {};
//------------------------------------------------------------------------------
// Connection info
//------------------------------------------------------------------------------
template <typename Base>
class paho_mqtt_connection_info : public Base {
public:
    auto kind() noexcept -> connection_kind final {
        return connection_kind::remote_interprocess;
    }

    auto addr_kind() noexcept -> connection_addr_kind final {
        return connection_addr_kind::string;
    }

    auto type_id() noexcept -> identifier final {
        return "PahoMQTT";
    }
};
//------------------------------------------------------------------------------
class paho_mqtt_connection
  : public main_ctx_object
  , public paho_mqtt_connection_info<connection> {
public:
    paho_mqtt_connection(
      main_ctx_parent parent,
      const shared_holder<paho_mqtt_common_state>&,
      const string_view addr_str) noexcept;

    using fetch_handler = connection::fetch_handler;

    auto update() noexcept -> work_done final;

    void cleanup() noexcept final;

    auto is_usable() noexcept -> bool final;

    auto max_data_size() noexcept -> valid_if_positive<span_size_t> final;

    auto send(const message_id msg_id, const message_view&) noexcept
      -> bool final;

    auto fetch_messages(const fetch_handler handler) noexcept
      -> work_done final;

    auto query_statistics(connection_statistics&) noexcept -> bool final;

    auto routing_weight() noexcept -> float final;

private:
};
//------------------------------------------------------------------------------
paho_mqtt_connection::paho_mqtt_connection(
  main_ctx_parent parent,
  const shared_holder<paho_mqtt_common_state>&,
  const string_view) noexcept
  : main_ctx_object{"PahoMQTTCn", parent} {}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::update() noexcept -> work_done {
    return {};
}
//------------------------------------------------------------------------------
void paho_mqtt_connection::cleanup() noexcept {}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::is_usable() noexcept -> bool {
    return true;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::max_data_size() noexcept
  -> valid_if_positive<span_size_t> {
    return {0};
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::send(
  const message_id msg_id,
  const message_view&) noexcept -> bool {
    return false;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::fetch_messages(const fetch_handler handler) noexcept
  -> work_done {
    (void)handler;
    some_true something_done;
    return something_done;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::query_statistics(connection_statistics&) noexcept
  -> bool {
    return false;
}
//------------------------------------------------------------------------------
auto paho_mqtt_connection::routing_weight() noexcept -> float {
    return 1.F;
}
//------------------------------------------------------------------------------
// Factory
//------------------------------------------------------------------------------
class paho_mqtt_connection_factory
  : public paho_mqtt_connection_info<connection_factory>
  , public main_ctx_object {
public:
    using connection_factory::make_acceptor;
    using connection_factory::make_connector;

    paho_mqtt_connection_factory(
      main_ctx_parent parent,
      shared_holder<paho_mqtt_common_state> paho_state) noexcept
      : main_ctx_object{"PahoConnFc", parent}
      , _paho_state{std::move(paho_state)} {}

    paho_mqtt_connection_factory(main_ctx_parent parent) noexcept
      : paho_mqtt_connection_factory{parent, default_selector} {}

    auto make_acceptor(const string_view) noexcept
      -> shared_holder<acceptor> final {
        return {};
    }

    auto make_connector(const string_view addr_str) noexcept
      -> shared_holder<connection> final {
        static_assert(not std::is_abstract_v<paho_mqtt_connection>);
        return {hold<paho_mqtt_connection>, *this, _paho_state, addr_str};
    }

private:
    shared_holder<paho_mqtt_common_state> _paho_state;
};
//------------------------------------------------------------------------------
// Factory functions
//------------------------------------------------------------------------------
auto make_paho_mqtt_connection_factory(main_ctx_parent parent)
  -> unique_holder<connection_factory> {
    return {hold<paho_mqtt_connection_factory>, parent};
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.core:connection_setup;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.logging;
import eagine.core.main_ctx;
import :types;
import :interface;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
export auto make_posix_mqueue_connection_factory(main_ctx_parent parent)
  -> unique_holder<connection_factory>;

export auto make_asio_tcp_ipv4_connection_factory(main_ctx_parent parent)
  -> unique_holder<connection_factory>;

export auto make_asio_udp_ipv4_connection_factory(main_ctx_parent parent)
  -> unique_holder<connection_factory>;

export auto make_asio_local_stream_connection_factory(main_ctx_parent parent)
  -> unique_holder<connection_factory>;

auto make_paho_mqtt_connection_factory(main_ctx_parent parent)
  -> unique_holder<connection_factory>;
//------------------------------------------------------------------------------
export class connection_setup;
export void connection_setup_configure(connection_setup&, application_config&);
//------------------------------------------------------------------------------
export auto adapt_entry_arg(
  const identifier name,
  const unique_holder<connection_factory>& value) noexcept;
//------------------------------------------------------------------------------
/// @brief Class setting up connections based from configuration
/// @ingroup msgbus
/// @see connection_kind
/// @see connection
/// @see acceptor
/// @see application_config
export class connection_setup : public main_ctx_object {
public:
    connection_setup(main_ctx_parent parent, const nothing_t) noexcept;

    /// @brief Construction from a parent main context object.
    connection_setup(main_ctx_parent parent) noexcept
      : connection_setup{parent, nothing} {
        configure(app_config());
    }

    /// @brief Sets up acceptors listening on the specified address.
    void setup_acceptors(acceptor_user& target, const string_view address);

    /// @brief Sets up acceptors listening on the specified address.
    void setup_acceptors(acceptor_user& target, const identifier address);

    /// @brief Sets up acceptors listening on the default address.
    void setup_acceptors(acceptor_user& target);

    /// @brief Sets up acceptors listening on the specified address.
    void setup_acceptors(
      acceptor_user& target,
      const connection_kinds kinds,
      const string_view address);

    /// @brief Sets up acceptors listening on the specified address.
    void setup_acceptors(
      acceptor_user& target,
      const connection_kinds kinds,
      const identifier address);

    /// @brief Sets up acceptors listening on the default address.
    void setup_acceptors(acceptor_user& target, const connection_kinds kinds);

    /// @brief Sets up acceptors listening on the specified address.
    void setup_acceptors(
      acceptor_user& target,
      const connection_kind kind,
      const string_view address);

    /// @brief Sets up acceptors listening on the specified address.
    void setup_acceptors(
      acceptor_user& target,
      const connection_kind kind,
      const identifier address);

    /// @brief Sets up acceptors listening on the default address.
    void setup_acceptors(acceptor_user& target, const connection_kind kind);

    /// @brief Sets up connectors connecting to the specified address.
    void setup_connectors(connection_user& target, const string_view address);

    /// @brief Sets up connectors connecting to the specified address.
    void setup_connectors(connection_user& target, const identifier address);

    /// @brief Sets up connectors connecting to the default address.
    void setup_connectors(connection_user& target);

    /// @brief Sets up connectors connecting to the specified address.
    void setup_connectors(
      connection_user& target,
      const connection_kinds kinds,
      const string_view address);

    /// @brief Sets up connectors connecting to the specified address.
    void setup_connectors(
      connection_user& target,
      const connection_kinds kinds,
      const identifier address);

    /// @brief Sets up connectors connecting to the default address.
    void setup_connectors(connection_user& target, const connection_kinds kinds);

    /// @brief Sets up connectors connecting to the specified address.
    void setup_connectors(
      connection_user& target,
      const connection_kind kind,
      const string_view address);

    /// @brief Sets up connectors connecting to the specified address.
    void setup_connectors(
      connection_user& target,
      const connection_kind kind,
      const identifier address);

    /// @brief Sets up connectors connecting to the default address.
    void setup_connectors(connection_user& target, const connection_kind kind);

    /// @brief Adds a new connection factory.
    void add_factory(unique_holder<connection_factory> factory);

    /// @brief Uses the configuration to do initialization of this setup.
    void configure(application_config& config) {
        connection_setup_configure(*this, config);
    }

private:
    std::mutex _mutex{};

    using _factory_list = std::vector<unique_holder<connection_factory>>;

    template <connection_kind Kind>
    using _enum_map_unit = _factory_list;

    static_enum_map<
      connection_kind,
      _enum_map_unit,
      connection_kind::in_process,
      connection_kind::local_interprocess,
      connection_kind::remote_interprocess>
      _factory_map{};

    void _do_setup_acceptors(
      acceptor_user& target,
      const string_view address,
      _factory_list& factories);

    void _do_setup_connectors(
      connection_user& target,
      const string_view address,
      _factory_list& factories);

    auto _make_call_setup_acceptors(
      acceptor_user& target,
      const string_view address) noexcept;

    auto _make_call_setup_connectors(
      connection_user& target,
      const string_view address) noexcept;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


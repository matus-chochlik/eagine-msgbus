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
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.main_ctx;
import :types;
import :direct;
import <mutex>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
void connection_setup::_do_setup_acceptors(
  acceptor_user& target,
  const string_view address,
  _factory_list& factories) {
    for(auto& factory : factories) {
        assert(factory);
        log_debug(
          "setting up acceptors on address ${address} "
          "with factory type ${factory}")
          .arg(identifier{"factory"}, factory)
          .arg(identifier{"address"}, identifier{"MsgBusAddr"}, address);

        if(auto acceptor{factory->make_acceptor(address)}) {
            target.add_acceptor(std::move(acceptor));
        }
    }
}
//------------------------------------------------------------------------------
void connection_setup::_do_setup_connectors(
  connection_user& target,
  const string_view address,
  _factory_list& factories) {
    for(auto& factory : factories) {
        assert(factory);
        log_debug(
          "setting up connectors on address ${address} "
          "with factory type ${factory}")
          .arg(identifier{"factory"}, factory)
          .arg(identifier{"address"}, identifier{"MsgBusAddr"}, address);

        if(auto connector{factory->make_connector(address)}) {
            target.add_connection(std::move(connector));
        }
    }
}
//------------------------------------------------------------------------------
auto connection_setup::_make_call_setup_acceptors(
  acceptor_user& target,
  const string_view address) {
    return [this, &target, address](auto, auto& factories) {
        _do_setup_acceptors(target, address, factories);
    };
}
//------------------------------------------------------------------------------
void connection_setup::setup_acceptors(
  acceptor_user& target,
  const string_view address) {
    std::unique_lock lock{_mutex};
    _factory_map.visit_all(_make_call_setup_acceptors(target, address));
}
//------------------------------------------------------------------------------
void connection_setup::setup_acceptors(
  acceptor_user& target,
  const connection_kinds kinds,
  const string_view address) {
    std::unique_lock lock{_mutex};
    _factory_map.visit(kinds, _make_call_setup_acceptors(target, address));
}
//------------------------------------------------------------------------------
void connection_setup::setup_acceptors(
  acceptor_user& target,
  const connection_kind kind,
  const string_view address) {
    std::unique_lock lock{_mutex};
    _factory_map.visit(kind, _make_call_setup_acceptors(target, address));
}
//------------------------------------------------------------------------------
auto connection_setup::_make_call_setup_connectors(
  connection_user& target,
  const string_view address) {
    return [this, &target, address](const auto, auto& factories) {
        _do_setup_connectors(target, address, factories);
    };
}
//------------------------------------------------------------------------------
void connection_setup::setup_connectors(
  connection_user& target,
  const string_view address) {
    std::unique_lock lock{_mutex};
    _factory_map.visit_all(_make_call_setup_connectors(target, address));
}
//------------------------------------------------------------------------------
void connection_setup::setup_connectors(
  connection_user& target,
  const connection_kinds kinds,
  const string_view address) {
    std::unique_lock lock{_mutex};
    _factory_map.visit(kinds, _make_call_setup_connectors(target, address));
}
//------------------------------------------------------------------------------
void connection_setup::setup_connectors(
  connection_user& target,
  const connection_kind kind,
  const string_view address) {
    std::unique_lock lock{_mutex};
    _factory_map.visit(kind, _make_call_setup_connectors(target, address));
}
//------------------------------------------------------------------------------
void connection_setup::add_factory(std::unique_ptr<connection_factory> factory) {
    std::unique_lock lock{_mutex};
    if(factory) {
        const auto kind{factory->kind()};

        log_debug("adding ${kind} connection factory ${factory}")
          .arg(identifier{"kind"}, kind)
          .arg(identifier{"addrKind"}, factory->addr_kind())
          .arg(identifier{"factory"}, factory);

        _factory_map.visit(
          kind, [factory{std::move(factory)}](auto, auto& factories) mutable {
              factories.emplace_back(std::move(factory));
          });
    }
}
//------------------------------------------------------------------------------
auto make_posix_mqueue_connection_factory(main_ctx_parent parent)
  -> std::unique_ptr<connection_factory>;

auto make_asio_tcp_ipv4_connection_factory(main_ctx_parent parent)
  -> std::unique_ptr<connection_factory>;

auto make_asio_udp_ipv4_connection_factory(main_ctx_parent parent)
  -> std::unique_ptr<connection_factory>;

auto make_asio_local_stream_connection_factory(main_ctx_parent parent)
  -> std::unique_ptr<connection_factory>;
//------------------------------------------------------------------------------
void connection_setup_configure(
  connection_setup& setup,
  application_config& config) {
    if(config.is_set("msgbus.asio_tcp_ipv4")) {
        setup.add_factory(make_asio_tcp_ipv4_connection_factory(setup));
    }
    if(config.is_set("msgbus.asio_udp_ipv4")) {
        setup.add_factory(make_asio_udp_ipv4_connection_factory(setup));
    }
    if(config.is_set("msgbus.asio_local_stream")) {
        setup.add_factory(make_asio_local_stream_connection_factory(setup));
    }
    if(config.is_set("msgbus.posix_mqueue")) {
        setup.add_factory(make_posix_mqueue_connection_factory(setup));
    }
    if(config.is_set("msgbus.direct")) {
        setup.add_factory(make_direct_connection_factory(setup));
    }
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

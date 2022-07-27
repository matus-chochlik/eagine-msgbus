/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.services:host_info;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.valid_if;
import eagine.core.main_ctx;
import eagine.msgbus.core;
import <array>;
import <chrono>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Service providing basic information about message bus endpoint's host.
/// @ingroup msgbus
/// @see service_composition
/// @see host_info_consumer
export template <typename Base = subscriber>
class host_info_provider : public Base {
    using This = host_info_provider;

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();

        Base::add_method(_host_id(
                           {"eagiSysInf", "hostId"},
                           &main_ctx::get().system(),
                           member_function_constant_t<&system_info::host_id>{})
                           .map_invoke_by({"eagiSysInf", "rqHostId"}));

        Base::add_method(_hostname(
                           {"eagiSysInf", "hostname"},
                           &main_ctx::get().system(),
                           member_function_constant_t<&system_info::hostname>{})
                           .map_invoke_by({"eagiSysInf", "rqHostname"}));
    }

private:
    default_function_skeleton<valid_if_positive<host_id_t>() noexcept, 64>
      _host_id;

    default_function_skeleton<valid_if_not_empty<std::string>() noexcept, 1024>
      _hostname;
};
//------------------------------------------------------------------------------
/// @brief Service consuming basic information about message bus endpoint's host.
/// @ingroup msgbus
/// @see service_composition
/// @see host_info_provider
export template <typename Base = subscriber>
class host_info_consumer : public Base {

    using This = host_info_consumer;

public:
    /// @brief Queries the endpoint's host identifier.
    /// @see host_id_received
    /// @see query_hostname
    void query_host_id(const identifier_t endpoint_id) noexcept {
        _host_id.invoke_on(
          this->bus_node(), endpoint_id, message_id{"eagiSysInf", "rqHostId"});
    }

    /// @brief Triggered on receipt of endpoint's host identifier.
    /// @see query_host_id
    signal<
      void(const result_context&, const valid_if_positive<host_id_t>&) noexcept>
      host_id_received;

    /// @brief Queries the endpoint's host name.
    /// @see hostname_received
    /// @see query_host_id
    void query_hostname(const identifier_t endpoint_id) noexcept {
        _hostname.invoke_on(
          this->bus_node(),
          endpoint_id,
          message_id{"eagiSysInf", "rqHostname"});
    }

    /// @brief Triggered on receipt of endpoint's host name.
    /// @see query_hostname
    signal<void(
      const result_context&,
      const valid_if_not_empty<std::string>&) noexcept>
      hostname_received;

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();

        Base::add_method(
          _host_id(host_id_received).map_fulfill_by({"eagiSysInf", "hostId"}));

        Base::add_method(_hostname(hostname_received)
                           .map_fulfill_by({"eagiSysInf", "hostname"}));
    }

private:
    default_callback_invoker<valid_if_positive<host_id_t>() noexcept, 32>
      _host_id;

    default_callback_invoker<valid_if_not_empty<std::string>() noexcept, 1024>
      _hostname;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.services:application_info;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.utility;
import eagine.core.valid_if;
import eagine.core.main_ctx;
import eagine.msgbus.core;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Service providing basic information about endpoint's application.
/// @ingroup msgbus
/// @see service_composition
/// @see application_info_consumer
export template <typename Base = subscriber>
class application_info_provider : public Base {
    using This = application_info_provider;

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();

        Base::add_method(_app_name(
                           {"eagiAppInf", "appName"},
                           &main_ctx::get(),
                           member_function_constant_t<&main_ctx::app_name>{})
                           .map_invoke_by({"eagiAppInf", "rqAppName"}));
    }

private:
    default_function_skeleton<string_view() noexcept, 256> _app_name;
};
//------------------------------------------------------------------------------
/// @brief Collection of signals emitted by the application info consumer service.
/// @ingroup msgbus
/// @see application_info_consumer
export struct application_info_consumer_signals {
    /// @brief Triggered on receipt of response about endpoint application name.
    /// @see query_application_name
    signal<void(
      const result_context&,
      const valid_if_not_empty<std::string>&) noexcept>
      application_name_received;
};
//------------------------------------------------------------------------------
/// @brief Service consuming basic information about endpoint's application.
/// @ingroup msgbus
/// @see service_composition
/// @see application_info_provider
export template <typename Base = subscriber>
class application_info_consumer
  : public Base
  , public application_info_consumer_signals {

    using This = application_info_consumer;

public:
    /// @brief Queries the specified endpoint's application name.
    /// @see application_name_received
    void query_application_name(const endpoint_id_t endpoint_id) noexcept {
        _app_name.invoke_on(
          this->bus_node(), endpoint_id, message_id{"eagiAppInf", "rqAppName"});
    }

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();

        Base::add_method(_app_name(this->application_name_received)
                           .map_fulfill_by({"eagiAppInf", "appName"}));
    }

private:
    default_callback_invoker<std::string() noexcept, 256> _app_name;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

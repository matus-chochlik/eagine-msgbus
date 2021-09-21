/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#ifndef EAGINE_MSGBUS_SERVICE_APPLICATION_INFO_HPP
#define EAGINE_MSGBUS_SERVICE_APPLICATION_INFO_HPP

#include "../service.hpp"
#include "../signal.hpp"
#include <eagine/bool_aggregate.hpp>
#include <eagine/main_ctx.hpp>
#include <eagine/maybe_unused.hpp>

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Service providing basic information about endpoint's application.
/// @ingroup msgbus
/// @see service_composition
/// @see application_info_consumer
template <typename Base = subscriber>
class application_info_provider : public Base {
    using This = application_info_provider;

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();

        Base::add_method(_app_name(
          EAGINE_MSG_ID(eagiAppInf, appName),
          &main_ctx::get(),
          EAGINE_MEM_FUNC_C(
            main_ctx, app_name))[EAGINE_MSG_ID(eagiAppInf, rqAppName)]);
    }

private:
    default_function_skeleton<string_view() noexcept, 256> _app_name;
};
//------------------------------------------------------------------------------
/// @brief Service consuming basic information about endpoint's application.
/// @ingroup msgbus
/// @see service_composition
/// @see application_info_provider
template <typename Base = subscriber>
class application_info_consumer : public Base {

    using This = application_info_consumer;

public:
    /// @brief Queries the specified endpoint's application name.
    /// @see application_name_received
    void query_application_name(const identifier_t endpoint_id) noexcept {
        _app_name.invoke_on(
          this->bus_node(), endpoint_id, EAGINE_MSG_ID(eagiAppInf, rqAppName));
    }

    /// @brief Triggered on receipt of response about endpoint application name.
    /// @see query_application_name
    signal<void(
      const result_context&,
      const valid_if_not_empty<std::string>&) noexcept>
      application_name_received;

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();

        Base::add_method(_app_name(
          application_name_received)[EAGINE_MSG_ID(eagiAppInf, appName)]);
    }

private:
    default_callback_invoker<std::string() noexcept, 256> _app_name;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

#endif // EAGINE_MSGBUS_SERVICE_APPLICATION_INFO_HPP

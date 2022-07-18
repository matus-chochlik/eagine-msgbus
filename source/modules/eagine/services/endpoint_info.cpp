/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.services:endpoint_info;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.main_ctx;
import eagine.msgbus.core;
import <array>;
import <chrono>;
import <string>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Service providing basic information about message bus endpoint.
/// @ingroup msgbus
/// @see service_composition
/// @see endpoint_info_consumer
/// @see endpoint_info
export template <typename Base = subscriber>
class endpoint_info_provider : public Base {

public:
    /// @brief Sets the endpoint info to be provided.
    auto provided_endpoint_info() noexcept -> endpoint_info& {
        return _info;
    }

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();

        Base::add_method(
          _respond(
            {"eagiEptInf", "response"},
            make_callable_ref<&endpoint_info_provider::_get_endpoint_info>(this))
            .map_invoke_by(("eagiEptInf", "request")));
    }

private:
    auto _get_endpoint_info() noexcept -> const endpoint_info& {
        return _info;
    }

    default_function_skeleton<const endpoint_info&() noexcept, 1024> _respond;
    endpoint_info _info;
};
//------------------------------------------------------------------------------
/// @brief Service consuming basic information about message bus endpoint.
/// @ingroup msgbus
/// @see service_composition
/// @see endpoint_info_provider
/// @see endpoint_info
export template <typename Base = subscriber>
class endpoint_info_consumer : public Base {

public:
    /// @brief Queries basic information about the specified endpoint.
    /// @see endpoint_info_received
    void query_endpoint_info(const identifier_t endpoint_id) noexcept {
        _info.invoke_on(
          this->bus_node(), endpoint_id, message_id{"eagiEptInf", "request"});
    }

    /// @brief Triggered on receipt of basic endpoint information.
    /// @see query_endpoint_info
    signal<void(const result_context&, const endpoint_info&) noexcept>
      endpoint_info_received;

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();

        Base::add_method(_info(endpoint_info_received)
                           .map_fulfill_by({"eagiEptInf", "response"}));
    }

private:
    default_callback_invoker<endpoint_info() noexcept, 1024> _info;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


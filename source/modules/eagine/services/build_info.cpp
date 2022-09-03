/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.services:build_info;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.build_info;
import eagine.core.identifier;
import eagine.core.valid_if;
import eagine.core.main_ctx;
import eagine.msgbus.core;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Service providing information about endpoint build version.
/// @ingroup msgbus
/// @see service_composition
/// @see build_info_consumer
export template <typename Base = subscriber>
class build_version_info_provider : public Base {
    using This = build_version_info_provider;

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();

        Base::add_method(_respond(
                           {"eagiBldInf", "response"},
                           &main_ctx::get(),
                           member_function_constant_t<&main_ctx::version>{})
                           .map_invoke_by({"eagiBldInf", "request"}));
    }

private:
    default_function_skeleton<const version_info&() noexcept, 256> _respond;
};
//------------------------------------------------------------------------------
/// @brief Collection of signals emitted by the build info provider service.
/// @ingroup msgbus
/// @see build_info_provider
/// @see build_info
export struct build_version_info_consumer_signals {
    /// @brief Triggered on receipt of endpoint's build version information.
    /// @see query_build_info
    signal<void(const result_context&, const version_info&) noexcept>
      build_version_info_received;
};
//------------------------------------------------------------------------------
/// @brief Service consuming information about endpoint build version.
/// @ingroup msgbus
/// @see service_composition
/// @see build_info_provider
/// @see build_info
template <typename Base = subscriber>
class build_version_info_consumer
  : public Base
  , public build_version_info_consumer_signals {

    using This = build_version_info_consumer;

public:
    /// @brief Queries endpoint's build version information.
    /// @see build_info_received
    void query_build_version_info(const identifier_t endpoint_id) noexcept {
        _build_version.invoke_on(
          this->bus_node(), endpoint_id, {"eagiBldInf", "request"});
    }

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();

        Base::add_method(_build_version(this->build_version_info_received)
                           .map_fulfill_by({"eagiBldInf", "response"}));
    }

private:
    default_callback_invoker<version_info() noexcept, 32> _build_version;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


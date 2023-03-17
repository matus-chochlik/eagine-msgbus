/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.services:compiler_info;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.build_info;
import eagine.core.identifier;
import eagine.core.utility;
import eagine.core.valid_if;
import eagine.core.main_ctx;
import eagine.msgbus.core;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Service providing information about endpoint compiler info.
/// @ingroup msgbus
/// @see service_composition
/// @see compiler_info_consumer
export template <typename Base = subscriber>
class compiler_info_provider : public Base {

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();

        Base::add_method(_respond(
                           {"eagiCplInf", "response"},
                           &main_ctx::get(),
                           member_function_constant_t<&main_ctx::compiler>{})
                           .map_invoke_by({"eagiCplInf", "request"}));
    }

private:
    default_function_skeleton<const compiler_info&() noexcept, 256> _respond;
};
//------------------------------------------------------------------------------
/// @brief Collection of signals emitted by the compiler info consumer service.
/// @ingroup msgbus
/// @see service_composition
/// @see compiler_info_provider
/// @see compiler_info
export struct compiler_info_consumer_signals {
    /// @brief Triggered on receipt of endpoints compiler information.
    /// @see query_compiler_info
    signal<void(const result_context&, const compiler_info&) noexcept>
      compiler_info_received;
};
//------------------------------------------------------------------------------
/// @brief Service consuming information about endpoint compiler info.
/// @ingroup msgbus
/// @see service_composition
/// @see compiler_info_provider
/// @see compiler_info
export template <typename Base = subscriber>
class compiler_info_consumer
  : public Base
  , public compiler_info_consumer_signals {

public:
    /// @brief Queries information about compiler used to build given endpoint.
    /// @see compiler_info_received
    void query_compiler_info(const identifier_t endpoint_id) noexcept {
        _compiler.invoke_on(
          this->bus_node(), endpoint_id, {"eagiCplInf", "request"});
    }

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();

        Base::add_method(_compiler(this->compiler_info_received)
                           .map_fulfill_by({"eagiCplInf", "response"}));
    }

private:
    default_callback_invoker<compiler_info() noexcept, 32> _compiler;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus


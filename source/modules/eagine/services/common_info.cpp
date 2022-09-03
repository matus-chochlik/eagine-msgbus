/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.services:common_info;

import eagine.core.types;
import eagine.msgbus.core;
export import :application_info;
export import :build_info;
export import :compiler_info;
export import :endpoint_info;
export import :host_info;

namespace eagine::msgbus {

/// @brief Alias for a common composition of information provider services.
/// @ingroup msgbus
/// @see service_composition
/// @see common_info_consumers
export template <typename Base = subscriber>
using common_info_providers = require_services<
  Base,
  compiler_info_provider,
  build_version_info_provider,
  host_info_provider,
  application_info_provider,
  endpoint_info_provider>;

/// @brief Alias for a common composition of information consumer services.
/// @ingroup msgbus
/// @see service_composition
/// @see common_info_providers
template <typename Base = subscriber>
using common_info_consumers = require_services<
  Base,
  compiler_info_consumer,
  build_version_info_consumer,
  host_info_consumer,
  application_info_consumer,
  endpoint_info_consumer>;

} // namespace eagine::msgbus


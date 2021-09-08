/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#ifndef EAGINE_MSGBUS_SERVICE_INTERFACE_HPP
#define EAGINE_MSGBUS_SERVICE_INTERFACE_HPP

#include <eagine/bool_aggregate.hpp>
#include <eagine/interface.hpp>
#include <eagine/types.hpp>

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Interface for message bus services
/// @ingroup msgbus
struct service_interface : interface<service_interface> {

    /// @brief Does an iteration update and processes all received messages.
    virtual auto update_and_process_all() noexcept -> work_done = 0;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

#endif // EAGINE_MSGBUS_SERVICE_INTERFACE_HPP

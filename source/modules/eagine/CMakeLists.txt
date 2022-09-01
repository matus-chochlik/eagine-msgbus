# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
add_subdirectory(core)
add_subdirectory(services)
add_subdirectory(utility)

eagine_add_module(
	eagine.msgbus
	PP_NAME MSGBUS
	INTERFACES msgbus
	IMPORTS
		eagine.msgbus.core
		eagine.msgbus.services
		eagine.msgbus.utility
)


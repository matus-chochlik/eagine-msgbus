# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
eagine_add_module(
	eagine.msgbus.services PARTITION ability
	IMPORTS
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.serialization
		eagine.core.valid_if
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION discovery
	IMPORTS
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION shutdown
	IMPORTS
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION topology
	IMPORTS
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.utility
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION ping_pong
	IMPORTS
		eagine.core.types
		eagine.core.debug
		eagine.core.memory
		eagine.core.identifier
		eagine.core.utility
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION statistics
	IMPORTS
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION application_info
	IMPORTS
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.valid_if
		eagine.core.main_ctx
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION endpoint_info
	IMPORTS
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.main_ctx
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services
	PP_NAME MSGBUS_SERVICES
	INTERFACES services
	PARTITIONS
		ability
		discovery
		shutdown
		topology
		ping_pong
		statistics
		application_info
		endpoint_info
	SOURCES
	IMPORTS
)

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
	eagine.msgbus.services PARTITION host_info
	IMPORTS
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.valid_if
		eagine.core.main_ctx
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION system_info
	IMPORTS
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.units
		eagine.core.valid_if
		eagine.core.main_ctx
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION compiler_info
	IMPORTS
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.valid_if
		eagine.core.main_ctx
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION build_info
	IMPORTS
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.valid_if
		eagine.core.main_ctx
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION common_info
	IMPORTS
		application_info build_info compiler_info
		host_info endpoint_info
		eagine.core.types
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION stream
	IMPORTS
		discovery ping_pong
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.container
		eagine.core.utility
		eagine.core.valid_if
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION resource_transfer
	IMPORTS
		discovery host_info
		eagine.core.types
		eagine.core.memory
		eagine.core.string
		eagine.core.identifier
		eagine.core.container
		eagine.core.serialization
		eagine.core.utility
		eagine.core.valid_if
		eagine.core.runtime
		eagine.core.math
		eagine.core.main_ctx
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION sudoku
	IMPORTS
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.container
		eagine.core.serialization
		eagine.core.utility
		eagine.core.runtime
		eagine.core.logging
		eagine.core.main_ctx
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION tracker
	IMPORTS
		common_info discovery host_info ping_pong
		statistics system_info topology
		eagine.core.types
		eagine.core.memory
		eagine.core.build_info
		eagine.core.identifier
		eagine.core.container
		eagine.core.utility
		eagine.core.units
		eagine.core.valid_if
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
		host_info
		system_info
		compiler_info
		build_info
		common_info
		stream
		resource_transfer
		sudoku
		tracker
	SOURCES
		discovery_impl
		topology_impl
		ping_pong_impl
		statistics_impl
		system_info_impl
		resource_transfer_impl
		tracker_impl
		sudoku_impl
	IMPORTS
)

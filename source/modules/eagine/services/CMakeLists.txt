# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
eagine_add_module(
	eagine.msgbus.services PARTITION ability
	IMPORTS
		std
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.serialization
		eagine.core.utility
		eagine.core.valid_if
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION discovery
	IMPORTS
		std
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.utility
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION shutdown
	IMPORTS
		std
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.utility
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION topology
	IMPORTS
		std
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.utility
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION ping_pong
	IMPORTS
		std
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
		std
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.utility
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION application_info
	IMPORTS
		std
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.utility
		eagine.core.valid_if
		eagine.core.main_ctx
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION endpoint_info
	IMPORTS
		std
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.utility
		eagine.core.main_ctx
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION host_info
	IMPORTS
		std
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.utility
		eagine.core.valid_if
		eagine.core.main_ctx
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION system_info
	IMPORTS
		std
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.units
		eagine.core.utility
		eagine.core.valid_if
		eagine.core.main_ctx
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION compiler_info
	IMPORTS
		std
		eagine.core.types
		eagine.core.memory
		eagine.core.build_info
		eagine.core.identifier
		eagine.core.utility
		eagine.core.valid_if
		eagine.core.main_ctx
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION build_info
	IMPORTS
		std
		eagine.core.types
		eagine.core.memory
		eagine.core.build_info
		eagine.core.identifier
		eagine.core.utility
		eagine.core.valid_if
		eagine.core.main_ctx
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION common_info
	IMPORTS
		std application_info build_info
		compiler_info host_info endpoint_info
		eagine.core.types
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION stream
	IMPORTS
		std discovery ping_pong
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.reflection
		eagine.core.container
		eagine.core.utility
		eagine.core.valid_if
		eagine.msgbus.core
)

eagine_add_module(
	eagine.msgbus.services PARTITION resource_transfer
	IMPORTS
		std discovery host_info
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
		std
		eagine.core.types
		eagine.core.memory
		eagine.core.math
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
		std common_info discovery host_info
		ping_pong statistics system_info topology
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
	SOURCES
		shutdown
		discovery
		topology
		ping_pong
		statistics
		system_info
		resource_transfer
		tracker
		sudoku
	IMPORTS
		std
		eagine.core.build_info
		eagine.core.container
		eagine.core.identifier
		eagine.core.logging
		eagine.core.main_ctx
		eagine.core.math
		eagine.core.memory
		eagine.core.runtime
		eagine.core.serialization
		eagine.core.string
		eagine.core.types
		eagine.core.units
		eagine.core.utility
		eagine.core.valid_if
		eagine.msgbus.core
)

eagine_add_module_tests(
	eagine.msgbus.services
	UNITS
		shutdown
		ping_pong
		discovery
		topology
		application_info
		compiler_info
		endpoint_info
		build_info
		host_info
		system_info
		common_info
		sudoku
	IMPORTS
		eagine.core
		eagine.core.main_ctx
		eagine.msgbus.core
)
set_tests_properties(execute-test.eagine.msgbus.services.ping_pong PROPERTIES COST 12)
set_tests_properties(execute-test.eagine.msgbus.services.discovery PROPERTIES COST 5)
set_tests_properties(execute-test.eagine.msgbus.services.topology PROPERTIES COST 5)
set_tests_properties(execute-test.eagine.msgbus.services.application_info PROPERTIES COST 2)
set_tests_properties(execute-test.eagine.msgbus.services.compiler_info PROPERTIES COST 2)
set_tests_properties(execute-test.eagine.msgbus.services.endpoint_info PROPERTIES COST 2)
set_tests_properties(execute-test.eagine.msgbus.services.build_info PROPERTIES COST 2)
set_tests_properties(execute-test.eagine.msgbus.services.host_info PROPERTIES COST 2)
set_tests_properties(execute-test.eagine.msgbus.services.system_info PROPERTIES COST 10)
set_tests_properties(execute-test.eagine.msgbus.services.common_info PROPERTIES COST 10)
set_tests_properties(execute-test.eagine.msgbus.services.sudoku PROPERTIES COST 160)

# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
eagine_add_module(
	eagine.msgbus.utility PARTITION resource_transfer
	IMPORTS
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.valid_if
		eagine.core.runtime
		eagine.core.main_ctx
		eagine.core.resource
		eagine.msgbus.core
		eagine.msgbus.services
)

eagine_add_module(
	eagine.msgbus.utility
	PP_NAME MSGBUS_UTILITY
	INTERFACES utility
	PARTITIONS
		resource_transfer
	SOURCES
		resource_transfer
	IMPORTS
		eagine.core.reflection
		eagine.core.valid_if
		eagine.core.logging
)


eagine_add_module_tests(
	eagine.msgbus.utility
	UNITS
		resource_transfer
	IMPORTS
		eagine.core
		eagine.msgbus.core
		eagine.msgbus.services
)
set_tests_properties(execute-test.eagine.msgbus.utility.resource_transfer PROPERTIES COST 10)


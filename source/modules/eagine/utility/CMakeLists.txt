# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt
eagine_add_module(
	eagine.msgbus.utility
	COMPONENT msgbus-dev
	PARTITION resource_transfer
	IMPORTS
		std
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.valid_if
		eagine.core.utility
		eagine.core.runtime
		eagine.core.main_ctx
		eagine.core.resource
		eagine.msgbus.core
		eagine.msgbus.services)

eagine_add_module(
	eagine.msgbus.utility
	COMPONENT msgbus-dev
	SOURCES
		resource_transfer
	IMPORTS
		std
		eagine.core.logging
		eagine.core.main_ctx
		eagine.core.memory
		eagine.core.reflection
		eagine.core.resource
		eagine.core.runtime
		eagine.core.types
		eagine.core.utility
		eagine.core.valid_if
		eagine.msgbus.core
		eagine.msgbus.services)


eagine_add_module_tests(
	eagine.msgbus.utility
	UNITS
		resource_transfer
	IMPORTS
		eagine.core
		eagine.msgbus.core
		eagine.msgbus.services)

set_tests_properties(execute-test.eagine.msgbus.utility.resource_transfer PROPERTIES COST 25)


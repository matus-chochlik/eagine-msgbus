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
		resource_transfer_impl
	IMPORTS
		eagine.core.logging
)


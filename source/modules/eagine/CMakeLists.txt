# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
eagine_add_module(
	eagine.msgbus PARTITION types
	IMPORTS
		eagine.core.types
		eagine.core.reflection
		eagine.core.identifier
)

eagine_add_module(
	eagine.msgbus PARTITION signal
	IMPORTS
		eagine.core.types
		eagine.core.container
		eagine.core.utility
)

eagine_add_module(
	eagine.msgbus PARTITION message
	IMPORTS
		types
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.reflection
		eagine.core.serialization
		eagine.core.utility
		eagine.core.main_ctx
)

eagine_add_module(
	eagine.msgbus PARTITION context
	IMPORTS
		types message
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.container
		eagine.core.main_ctx
		eagine.sslplus
)

eagine_add_module(
	eagine.msgbus PARTITION router_address
	IMPORTS
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.logging
		eagine.core.main_ctx
)

eagine_add_module(
	eagine.msgbus PARTITION interface
	IMPORTS
		types message
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.valid_if
		eagine.core.utility
)

eagine_add_module(
	eagine.msgbus PARTITION blobs
	IMPORTS
		types message
		eagine.core.types
		eagine.core.debug
		eagine.core.memory
		eagine.core.identifier
		eagine.core.serialization
		eagine.core.valid_if
		eagine.core.utility
		eagine.core.main_ctx
)

eagine_add_module(
	eagine.msgbus PARTITION loopback
	IMPORTS
		types message interface
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.utility
)

eagine_add_module(
	eagine.msgbus PARTITION direct
	IMPORTS
		types message interface
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.utility
		eagine.core.main_ctx
)

eagine_add_module(
	eagine.msgbus
	PP_NAME MSGBUS
	INTERFACES msgbus
	PARTITIONS
		types
		signal
		message
		context
		router_address
		interface
		blobs
		loopback
		direct
	SOURCES
		message_impl
		context_impl
		blobs_impl
)


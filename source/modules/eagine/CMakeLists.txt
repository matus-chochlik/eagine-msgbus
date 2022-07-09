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
	eagine.msgbus PARTITION future
	IMPORTS
		types
		eagine.core.types
		eagine.core.container
		eagine.core.utility
)

eagine_add_module(
	eagine.msgbus PARTITION handler_map
	IMPORTS
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
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
	eagine.msgbus PARTITION endpoint
	IMPORTS
		types blobs signal message
		interface context
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.container
		eagine.core.utility
		eagine.core.valid_if
		eagine.core.main_ctx
)

eagine_add_module(
	eagine.msgbus PARTITION invoker
	IMPORTS
		types future handler_map
		message endpoint
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.serialization
		eagine.core.utility
)

eagine_add_module(
	eagine.msgbus PARTITION skeleton
	IMPORTS
		types message handler_map
		endpoint
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.serialization
		eagine.core.utility
		eagine.core.runtime
)

eagine_add_module(
	eagine.msgbus PARTITION subscriber
	IMPORTS
		types message handler_map
		endpoint
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.utility
		eagine.core.main_ctx
)

eagine_add_module(
	eagine.msgbus PARTITION remote_node
	IMPORTS
		types
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.reflection
		eagine.core.container
		eagine.core.units
		eagine.core.utility
		eagine.core.valid_if
		eagine.core.runtime
		eagine.core.main_ctx
)

eagine_add_module(
	eagine.msgbus
	PP_NAME MSGBUS
	INTERFACES msgbus
	PARTITIONS
		types
		signal
		future
		handler_map
		message
		context
		router_address
		interface
		blobs
		loopback
		direct
		endpoint
		invoker
		skeleton
		subscriber
		remote_node
	SOURCES
		message_impl
		context_impl
		blobs_impl
		endpoint_impl
		remote_node_impl
)


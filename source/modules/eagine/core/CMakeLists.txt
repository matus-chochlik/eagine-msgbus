# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
configure_file(embedded.cpp.in embedded.cpp @ONLY)

eagine_add_module(
	eagine.msgbus.core PARTITION types
	IMPORTS
		eagine.core.types
		eagine.core.memory
		eagine.core.string
		eagine.core.reflection
		eagine.core.identifier
)

eagine_add_module(
	eagine.msgbus.core PARTITION signal
	IMPORTS
		eagine.core.types
		eagine.core.container
		eagine.core.utility
)

eagine_add_module(
	eagine.msgbus.core PARTITION future
	IMPORTS
		types
		eagine.core.types
		eagine.core.container
		eagine.core.utility
)

eagine_add_module(
	eagine.msgbus.core PARTITION handler_map
	IMPORTS
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
)

eagine_add_module(
	eagine.msgbus.core PARTITION message
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
	eagine.msgbus.core PARTITION context
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
	eagine.msgbus.core PARTITION router_address
	IMPORTS
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.logging
		eagine.core.main_ctx
)

eagine_add_module(
	eagine.msgbus.core PARTITION interface
	IMPORTS
		types message
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.valid_if
		eagine.core.utility
)

eagine_add_module(
	eagine.msgbus.core PARTITION blobs
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
	eagine.msgbus.core PARTITION loopback
	IMPORTS
		types message interface
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.utility
)

eagine_add_module(
	eagine.msgbus.core PARTITION direct
	IMPORTS
		types message interface
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.utility
		eagine.core.main_ctx
)

eagine_add_module(
	eagine.msgbus.core PARTITION connection_setup
	IMPORTS
		types
		interface
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
)

eagine_add_module(
	eagine.msgbus.core PARTITION endpoint
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
	eagine.msgbus.core PARTITION invoker
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
	eagine.msgbus.core PARTITION skeleton
	IMPORTS
		types message handler_map
		endpoint
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.container
		eagine.core.serialization
		eagine.core.utility
		eagine.core.runtime
)

eagine_add_module(
	eagine.msgbus.core PARTITION subscriber
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
	eagine.msgbus.core PARTITION actor
	IMPORTS
		types message handler_map
		endpoint subscriber
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.utility
		eagine.core.main_ctx
)

eagine_add_module(
	eagine.msgbus.core PARTITION router
	IMPORTS
		types message blobs
		interface context
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.container
		eagine.core.valid_if
		eagine.core.utility
		eagine.core.runtime
		eagine.core.main_ctx
)

eagine_add_module(
	eagine.msgbus.core PARTITION bridge
	IMPORTS
		types message
		interface context
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.container
		eagine.core.valid_if
		eagine.core.utility
		eagine.core.main_ctx
)

eagine_add_module(
	eagine.msgbus.core PARTITION service
	IMPORTS
		types interface message
		invoker skeleton
		endpoint subscriber
		handler_map
		eagine.core.types
		eagine.core.identifier
		eagine.core.serialization
		eagine.core.utility
		eagine.core.main_ctx
)

eagine_add_module(
	eagine.msgbus.core PARTITION setup
	IMPORTS
		types interface
		router_address connection_setup
		eagine.core.types
		eagine.core.identifier
		eagine.core.main_ctx
)

eagine_add_module(
	eagine.msgbus.core PARTITION registry
	IMPORTS
		types interface setup
		direct endpoint router
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.utility
		eagine.core.main_ctx
)

eagine_add_module(
	eagine.msgbus.core PARTITION remote_node
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
		eagine.core.logging
		eagine.core.main_ctx
)

eagine_add_module(
	eagine.msgbus.core PARTITION resources
	IMPORTS
		eagine.core.types
		eagine.core.memory
		eagine.core.logging
		eagine.core.main_ctx
		eagine.core.resource
)

eagine_add_module(
	eagine.msgbus.core PARTITION embedded
	IMPORTS
		resources
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.resource
)
eagine_embed_target_resources(TARGET eagine.msgbus.core.embedded)

eagine_add_module(
	eagine.msgbus.core
	PP_NAME MSGBUS_CORE
	INTERFACES core
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
		connection_setup
		endpoint
		invoker
		skeleton
		subscriber
		actor
		router
		bridge
		service
		setup
		registry
		remote_node
		resources
		embedded
	SOURCES
		message_impl
		context_impl
		blobs_impl
		connection_setup_impl
		posix_mqueue_impl
		asio_impl
		endpoint_impl
		router_impl
		bridge_impl
		registry_impl
		remote_node_impl
	IMPORTS
		eagine.core.debug
		eagine.core.container
		eagine.core.reflection
		eagine.core.serialization
		eagine.core.valid_if
		eagine.core.utility
	PRIVATE_INCLUDE_DIRECTORIES
		"${PROJECT_SOURCE_DIR}/submodules/asio/asio/include"
)


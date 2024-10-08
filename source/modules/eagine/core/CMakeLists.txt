# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt
configure_file(embedded.cpp.in embedded.cppm @ONLY)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION types
	IMPORTS
		std
		eagine.core.types
		eagine.core.memory
		eagine.core.string
		eagine.core.reflection
		eagine.core.identifier
		eagine.core.logging)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION future
	IMPORTS
		std types
		eagine.core.types
		eagine.core.container
		eagine.core.utility)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION handler_map
	IMPORTS
		std
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION message
	IMPORTS
		std types
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.reflection
		eagine.core.serialization
		eagine.core.utility
		eagine.core.runtime
		eagine.core.valid_if
		eagine.core.main_ctx)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION context
	IMPORTS
		std types message
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.container
		eagine.core.main_ctx
		eagine.sslplus)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION router_address
	IMPORTS
		std
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.container
		eagine.core.logging
		eagine.core.main_ctx)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION interface
	IMPORTS
		std types message
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.valid_if
		eagine.core.utility)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION blobs
	IMPORTS
		std types message
		eagine.core.types
		eagine.core.debug
		eagine.core.memory
		eagine.core.identifier
		eagine.core.serialization
		eagine.core.valid_if
		eagine.core.utility
		eagine.core.main_ctx)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION loopback
	IMPORTS
		std types message interface
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.utility
		eagine.core.main_ctx)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION direct
	IMPORTS
		std types message interface
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.container
		eagine.core.utility
		eagine.core.main_ctx)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION connection_setup
	IMPORTS
		std types interface
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.logging
		eagine.core.main_ctx)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION endpoint
	IMPORTS
		std types blobs message
		interface context
		eagine.core.build_config
		eagine.core.types
		eagine.core.debug
		eagine.core.memory
		eagine.core.identifier
		eagine.core.container
		eagine.core.valid_if
		eagine.core.utility
		eagine.core.logging
		eagine.core.main_ctx)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION invoker
	IMPORTS
		std types future handler_map
		message endpoint
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.serialization
		eagine.core.utility)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION skeleton
	IMPORTS
		std types message
		handler_map endpoint
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.container
		eagine.core.serialization
		eagine.core.utility
		eagine.core.runtime)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION subscriber
	IMPORTS
		std types message
		handler_map endpoint
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.utility
		eagine.core.main_ctx)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION actor
	IMPORTS
		std types message handler_map
		endpoint subscriber
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.utility
		eagine.core.main_ctx)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION router
	IMPORTS
		std types message blobs
		interface context
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.container
		eagine.core.valid_if
		eagine.core.utility
		eagine.core.runtime
		eagine.core.main_ctx)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION bridge
	IMPORTS
		std types message
		interface context
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.container
		eagine.core.valid_if
		eagine.core.utility
		eagine.core.main_ctx)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION mqtt_bridge
	IMPORTS
		std types message
		interface context
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.container
		eagine.core.valid_if
		eagine.core.utility
		eagine.core.runtime
		eagine.core.main_ctx)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION service
	IMPORTS
		std types interface message
		invoker skeleton
		endpoint subscriber
		handler_map
		eagine.core.types
		eagine.core.identifier
		eagine.core.serialization
		eagine.core.utility
		eagine.core.main_ctx)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION setup
	IMPORTS
		std types interface
		router_address connection_setup
		eagine.core.types
		eagine.core.identifier
		eagine.core.main_ctx)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION optional_router
	IMPORTS
		std router
		eagine.core.memory
		eagine.core.utility
		eagine.core.main_ctx)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION registry
	IMPORTS
		std types interface setup direct
		service endpoint router
		eagine.core.types
		eagine.core.memory
		eagine.core.identifier
		eagine.core.utility
		eagine.core.main_ctx)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION remote_node
	IMPORTS
		std types
		eagine.core.types
		eagine.core.build_info
		eagine.core.memory
		eagine.core.identifier
		eagine.core.reflection
		eagine.core.container
		eagine.core.units
		eagine.core.utility
		eagine.core.valid_if
		eagine.core.runtime
		eagine.core.logging
		eagine.core.main_ctx)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION resources
	IMPORTS
		std
		eagine.core)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	PARTITION embedded
	IMPORTS
		std resources
		eagine.core)

eagine_embed_target_resources(TARGET eagine.msgbus.core)

eagine_add_module(
	eagine.msgbus.core
	COMPONENT msgbus-dev
	SOURCES
		message
		context
		blobs
		setup
		connection_setup
		router_address
		posix_mqueue
		paho_mqtt
		asio
		endpoint
		router
		bridge
		mqtt_bridge
		registry
		optional_router
		remote_node
	IMPORTS
		std
		eagine.core.build_config
		eagine.core.build_info
		eagine.core.types
		eagine.core.debug
		eagine.core.memory
		eagine.core.string
		eagine.core.container
		eagine.core.identifier
		eagine.core.reflection
		eagine.core.serialization
		eagine.core.valid_if
		eagine.core.runtime
		eagine.core.utility
		eagine.core.units
		eagine.core.math
		eagine.core.c_api
		eagine.core.logging
		eagine.core.main_ctx
		eagine.core.resource
		eagine.sslplus
	PRIVATE_INCLUDE_DIRECTORIES
		"${PROJECT_SOURCE_DIR}/submodules/asio/asio/include"
		"${PROJECT_SOURCE_DIR}/submodules/paho.mqtt.c/src"
	PRIVATE_LINK_LIBRARIES
		"${PROJECT_BINARY_DIR}/submodules/paho.mqtt.c/src/${CMAKE_STATIC_LIBRARY_PREFIX}paho-mqtt3a${CMAKE_STATIC_LIBRARY_SUFFIX}"
		"${PROJECT_BINARY_DIR}/submodules/paho.mqtt.c/src/${CMAKE_STATIC_LIBRARY_PREFIX}paho-mqtt3c${CMAKE_STATIC_LIBRARY_SUFFIX}")

eagine_add_module_tests(
	eagine.msgbus.core
	UNITS
		types
		message
		loopback
		direct
		asio
		posix_mqueue
		blobs
		endpoint
		actor
		registry
	IMPORTS
		std
		eagine.core
		eagine.core.main_ctx)
set_tests_properties(execute-test.eagine.msgbus.core.loopback PROPERTIES COST 20)
set_tests_properties(execute-test.eagine.msgbus.core.direct PROPERTIES COST 20)
set_tests_properties(execute-test.eagine.msgbus.core.posix_mqueue PROPERTIES COST 10)
set_tests_properties(execute-test.eagine.msgbus.core.asio PROPERTIES COST 35)
set_tests_properties(execute-test.eagine.msgbus.core.message PROPERTIES COST 55)
set_tests_properties(execute-test.eagine.msgbus.core.blobs PROPERTIES COST 70)
set_tests_properties(execute-test.eagine.msgbus.core.actor PROPERTIES COST 10)
set_tests_properties(execute-test.eagine.msgbus.core.registry PROPERTIES COST 30)

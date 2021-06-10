# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt

add_library(
	eagine-msgbus
	message_bus_context.cpp
	message_bus_message.cpp
	message_bus_setup.cpp
	message_bus_blobs.cpp
	message_bus_endpoint.cpp
	message_bus_remote_node.cpp
	message_bus_registry.cpp
	message_bus_router.cpp
	message_bus_bridge.cpp
)
eagine_add_lib_analysis(eagine-msgbus)

target_compile_definitions(
	eagine-msgbus PUBLIC EAGINE_MSGBUS_LIBRARY=1
)

target_include_directories(
	eagine-msgbus
	PRIVATE
		"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
)

target_link_libraries(
	eagine-msgbus
	PUBLIC
		eagine-msgbus-headers
		EAGine::Core
		EAGine::SSL
	PRIVATE
		eagine-msgbus-private-headers
)

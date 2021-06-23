# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt

add_library(
	eagine-msgbus
	resources.cpp
	context.cpp
	message.cpp
	setup.cpp
	blobs.cpp
	endpoint.cpp
	remote_node.cpp
	registry.cpp
	router.cpp
	bridge.cpp
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
		EAGine::SSLplus
	PRIVATE
		eagine-msgbus-generated-headers
		eagine-msgbus-private-headers
)

# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
#
configure_file(
	eagine/message_bus/resources.gen.in
	eagine/message_bus/resources.gen.inl
	@ONLY
)
include(./Sources.cmake)
set(GEN_INLS eagine/message_bus/resources.gen.inl)
# - regular headers ------------------------------------------------------------
add_library(eagine-msgbus-headers INTERFACE)

target_include_directories(
	eagine-msgbus-headers
	INTERFACE
		$<INSTALL_INTERFACE:include>
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)

foreach(FILENAME ${HEADERS} ${PUB_INLS})
	set_property(
		TARGET eagine-msgbus-headers
		APPEND PROPERTY PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME}
	)
endforeach()
# - private headers or implementation files ------------------------------------
add_library(eagine-msgbus-private-headers INTERFACE)

target_include_directories(
	eagine-msgbus-private-headers
	INTERFACE
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		"$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/submodules/asio/asio/include>"
)

foreach(FILENAME ${HEADERS} ${LIB_INLS})
	set_property(
		TARGET eagine-msgbus-private-headers
		APPEND PROPERTY PRIVATE_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME}
	)
endforeach()
# - generated headers or implementation files ----------------------------------
add_library(eagine-msgbus-generated-headers INTERFACE)

target_include_directories(
	eagine-msgbus-generated-headers
	INTERFACE
	$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
)

foreach(FILENAME ${GEN_INLS})
	set_property(
		TARGET eagine-msgbus-generated-headers
		APPEND PROPERTY PRIVATE_HEADER ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}
	)
endforeach()


# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
#
file(GLOB_RECURSE HEADERS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)
file(GLOB_RECURSE LIB_INLS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.inl)
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


# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
#
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/asio/asio")
	execute_process(
		COMMAND git submodule update --init --recursive
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/asio"
	)
endif()

if(NOT TARGET EAGine::Core)
	if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/eagine-core/CMakeLists.txt")
		execute_process(
			COMMAND git submodule update --init
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/eagine-core"
		)
	endif()
	add_subdirectory(eagine-core)
endif()
if(NOT TARGET EAGine::SSL)
	if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/eagine-ssl/CMakeLists.txt")
		execute_process(
			COMMAND git submodule update --init
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/eagine-ssl"
		)
	endif()
	add_subdirectory(eagine-ssl)
endif()

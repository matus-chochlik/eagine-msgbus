# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt

# router
add_subdirectory(router)

# bridge
add_subdirectory(bridge)

# pingable
add_subdirectory(pingable)

# pinger
add_subdirectory(pinger)

# file server
add_subdirectory(file_server)

# resource clients
add_subdirectory(resource_hexdump)
add_subdirectory(resource_histogram)

# sudoku solver helper
add_subdirectory(sudoku_helper)

# sudoku tiling generator
add_executable(eagine-msgbus-sudoku-tiling sudoku_tiling.cpp)
eagine_add_exe_analysis(eagine-msgbus-sudoku-tiling)
eagine_target_modules(
	eagine-msgbus-sudoku-tiling
	std
	eagine.core
	eagine.sslplus
	eagine.msgbus
)

# scripts
install(
	PROGRAMS eagine-local-bridge.sh
	RENAME eagine-local-bridge
	DESTINATION bin
	COMPONENT tools
)
install(
	PROGRAMS eagine-sudoku-tiling.sh
	RENAME eagine-sudoku-tiling
	DESTINATION bin
	COMPONENT tools
)

find_package(Qt5 COMPONENTS Core Widgets Qml Quick)
if(Qt5_FOUND)
	set(CMAKE_AUTOMOC ON)
	set(CMAKE_AUTORCC ON)
	add_subdirectory(monitor)
	add_subdirectory(sudoku_tiling_gui)
endif()

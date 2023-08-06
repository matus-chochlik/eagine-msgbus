# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt

# router
add_executable(eagine-msgbus-router router.cpp)
eagine_add_exe_analysis(eagine-msgbus-router)
eagine_target_modules(
	eagine-msgbus-router
	std
	eagine.core
	eagine.sslplus
	eagine.msgbus
)

# bridge
add_executable(eagine-msgbus-bridge bridge.cpp)
eagine_add_exe_analysis(eagine-msgbus-bridge)
eagine_target_modules(
	eagine-msgbus-bridge
	std
	eagine.core
	eagine.sslplus
	eagine.msgbus
)

# pingable
add_executable(eagine-msgbus-pingable pingable.cpp)
eagine_add_exe_analysis(eagine-msgbus-pingable)
eagine_target_modules(
	eagine-msgbus-pingable
	std
	eagine.core
	eagine.sslplus
	eagine.msgbus
)

# pinger
add_executable(eagine-msgbus-pinger pinger.cpp)
eagine_add_exe_analysis(eagine-msgbus-pinger)
eagine_target_modules(
	eagine-msgbus-pinger
	std
	eagine.core
	eagine.sslplus
	eagine.msgbus
)

# file server
add_executable(eagine-msgbus-file_server file_server.cpp)
eagine_add_exe_analysis(eagine-msgbus-file_server)
eagine_target_modules(
	eagine-msgbus-file_server
	std
	eagine.core
	eagine.sslplus
	eagine.msgbus
)

# sudoku solver helper
add_executable(eagine-msgbus-sudoku_helper sudoku_helper.cpp)
eagine_add_exe_analysis(eagine-msgbus-sudoku_helper)
eagine_target_modules(
	eagine-msgbus-sudoku_helper
	std
	eagine.core
	eagine.sslplus
	eagine.msgbus
)

# sudoku tiling generator
add_executable(eagine-msgbus-sudoku_tiling sudoku_tiling.cpp)
eagine_add_exe_analysis(eagine-msgbus-sudoku_tiling)
eagine_target_modules(
	eagine-msgbus-sudoku_tiling
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
	add_subdirectory(tiling)
endif()

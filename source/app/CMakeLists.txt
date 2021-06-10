# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt

# router
add_executable(eagine-msgbus-router router.cpp)
eagine_add_exe_analysis(eagine-msgbus-router)
target_link_libraries(
	eagine-msgbus-router
	PUBLIC EAGine::MsgBus
)

# bridge
add_executable(eagine-msgbus-bridge bridge.cpp)
eagine_add_exe_analysis(eagine-msgbus-bridge)
target_link_libraries(
	eagine-msgbus-bridge
	PUBLIC EAGine::MsgBus
)

# pingable
add_executable(eagine-msgbus-pingable pingable.cpp)
eagine_add_exe_analysis(eagine-msgbus-pingable)
target_link_libraries(
	eagine-msgbus-pingable
	PUBLIC EAGine::MsgBus
)

# pinger
add_executable(eagine-msgbus-pinger pinger.cpp)
eagine_add_exe_analysis(eagine-msgbus-pinger)
target_link_libraries(
	eagine-msgbus-pinger
	PUBLIC EAGine::MsgBus
)

# file server
add_executable(eagine-msgbus-file_server file_server.cpp)
eagine_add_exe_analysis(eagine-msgbus-file_server)
target_link_libraries(
	eagine-msgbus-file_server
	PUBLIC EAGine::MsgBus
)

# sudoku solver helper
add_executable(eagine-msgbus-sudoku_helper sudoku_helper.cpp)
eagine_add_exe_analysis(eagine-msgbus-sudoku_helper)
target_link_libraries(
	eagine-msgbus-sudoku_helper
	PUBLIC EAGine::MsgBus
)

# sudoku tiling generator
add_executable(eagine-msgbus-sudoku_tiling sudoku_tiling.cpp)
eagine_add_exe_analysis(eagine-msgbus-sudoku_tiling)
target_link_libraries(
	eagine-msgbus-sudoku_tiling
	PUBLIC EAGine::MsgBus
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

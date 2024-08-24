# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt

# router
add_subdirectory(router)

# bridge
add_subdirectory(bridge)

# MQTT bridge
add_subdirectory(mqtt_bridge)

# pingable
add_subdirectory(pingable)

# pinger
add_subdirectory(pinger)

# file server
add_subdirectory(file_server)

# resource clients
add_subdirectory(resource_get)
add_subdirectory(resource_hexdump)
add_subdirectory(resource_histogram)

# sudoku solver helper
add_subdirectory(sudoku_helper)

# sudoku tiling generator
add_subdirectory(sudoku_tiling)

# scripts
install(
	PROGRAMS eagine-local-bridge.sh
	COMPONENT msgbus-apps
	RENAME eagine-local-bridge
	DESTINATION bin
)

find_package(Qt5 COMPONENTS Core Widgets Qml Quick)
if(Qt5_FOUND)
	set(CMAKE_AUTOMOC ON)
	set(CMAKE_AUTORCC ON)
	add_subdirectory(monitor)
	add_subdirectory(sudoku_tiling_gui)
endif()

eagine_add_license(msgbus-apps)
eagine_add_debian_changelog(msgbus-apps)

eagine_add_package_property(
	msgbus-apps
	GENERATOR DEBIAN
	VARIABLE PACKAGE_DEPENDS
	VALUE "\${CXX_RUNTIME_PKGS},\${EAGINE_CORE_RUNTIME_PKGS},\${EAGINE_SSLPLUS_RUNTIME_PKGS},eagine-user-account")

eagine_add_package_property(
	msgbus-apps
	GENERATOR DEBIAN
	VARIABLE PACKAGE_SUGGESTS
	VALUE "eagine-core-apps")

eagine_add_package_property(
	msgbus-apps
	GENERATOR DEBIAN
	VARIABLE DESCRIPTION
	VALUE "Collection of EAGine msgbus applications.")


# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt
add_subdirectory(core)
add_subdirectory(services)
add_subdirectory(utility)

eagine_add_module(
	eagine.msgbus
	COMPONENT msgbus-dev
	SUBMODULES
		core
		services
		utility)

eagine_add_license(msgbus-dev)
eagine_add_debian_changelog(msgbus-dev)

eagine_add_package_property(
	msgbus-dev
	GENERATOR DEBIAN
	VARIABLE PACKAGE_DEPENDS
	VALUE "eagine-core-dev (>= ${EAGINE_VERSION}),eagine-sslplus-dev (>= ${EAGINE_VERSION})")

eagine_add_package_property(
	msgbus-dev
	GENERATOR DEBIAN
	VARIABLE DESCRIPTION
	VALUE "C++ implementation of an inter-process network message bus.")


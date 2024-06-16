# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt
add_subdirectory(modules)
add_subdirectory(app)
add_subdirectory(benchmark)

if(TARGET eagine-msgbus-monitor)
	# TODO: re-enable this when clang is fixed
	# list(APPEND EAGINE_MSGBUS_EXPORT_TARGETS eagine-msgbus-monitor)
endif()

if(TARGET eagine-msgbus-tiling)
	# TODO: re-enable this when clang is fixed
	# list(APPEND EAGINE_MSGBUS_EXPORT_TARGETS eagine-msgbus-tiling)
endif()


# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
add_subdirectory(modules)
add_subdirectory(config)
add_subdirectory(app)
add_subdirectory(benchmark)

set(
	EAGINE_MSGBUS_EXPORT_TARGETS
	eagine-msgbus-router
	eagine-msgbus-bridge
	eagine-msgbus-pingable
	eagine-msgbus-pinger
	eagine-msgbus-file-server
	eagine-msgbus-resource-hexdump
	eagine-msgbus-resource-histogram
	eagine-msgbus-sudoku-helper
	eagine-msgbus-sudoku-tiling
)

if(TARGET eagine-msgbus-monitor)
	# TODO: re-enable this when clang is fixed
	# list(APPEND EAGINE_MSGBUS_EXPORT_TARGETS eagine-msgbus-monitor)
endif()

if(TARGET eagine-msgbus-tiling)
	# TODO: re-enable this when clang is fixed
	# list(APPEND EAGINE_MSGBUS_EXPORT_TARGETS eagine-msgbus-tiling)
endif()

install(
	TARGETS ${EAGINE_MSGBUS_EXPORT_TARGETS}
	EXPORT MsgBus
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	RUNTIME DESTINATION bin
	INCLUDES DESTINATION include
	PUBLIC_HEADER DESTINATION include
)


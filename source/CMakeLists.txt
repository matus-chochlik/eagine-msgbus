# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
if(${WITH_MODULES})
	add_subdirectory(modules)
else()
	add_subdirectory(lib)
endif()
add_subdirectory(config)
add_subdirectory(app)

	set(
		EAGINE_MSGBUS_EXPORT_TARGETS
		eagine-msgbus-router
		eagine-msgbus-bridge
		eagine-msgbus-pingable
		eagine-msgbus-pinger
		eagine-msgbus-file_server
		eagine-msgbus-sudoku_helper
		eagine-msgbus-sudoku_tiling
	)

if(TARGET eagine-msgbus)
	add_library(EAGine::MsgBus INTERFACE IMPORTED GLOBAL)
	target_link_libraries(EAGine::MsgBus INTERFACE eagine-msgbus)

	list(
		APPEND EAGINE_MSGBUS_EXPORT_TARGETS
		eagine-msgbus-generated-headers
		eagine-msgbus-private-headers
		eagine-msgbus-headers
		eagine-msgbus
	)

	install(
		EXPORT MsgBus
		NAMESPACE EAGine::
		FILE MsgBus.cmake
		DESTINATION lib/cmake/EAGine
	)
endif()

if(TARGET eagine-msgbus-monitor)
	list(APPEND EAGINE_MSGBUS_EXPORT_TARGETS eagine-msgbus-monitor)
endif()

if(TARGET eagine-msgbus-tiling)
	list(APPEND EAGINE_MSGBUS_EXPORT_TARGETS eagine-msgbus-tiling)
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


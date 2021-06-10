# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt

add_subdirectory(lib)
add_subdirectory(app)

add_library(EAGine::MsgBus INTERFACE IMPORTED GLOBAL)
target_link_libraries(EAGine::MsgBus INTERFACE eagine-msgbus)

install(
	TARGETS
		eagine-msgbus-private-headers
		eagine-msgbus-headers
		eagine-msgbus
		EXPORT MsgBus
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	RUNTIME DESTINATION bin
	INCLUDES DESTINATION include
	PUBLIC_HEADER DESTINATION include
)

install(
	EXPORT MsgBus
	NAMESPACE EAGine::
	FILE MsgBus.cmake
	DESTINATION lib/cmake/EAGine
)

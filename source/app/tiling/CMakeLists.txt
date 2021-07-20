# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt

# tiling
set(TILING_RESOURCES
	tiling.qrc
)
set(TILING_SOURCES
	tiling.cpp
	src/HelperContributionViewModel.cpp
	src/SolutionIntervalViewModel.cpp
	src/TilingTheme.cpp
	src/TilingBackend.cpp
	src/TilingModel.cpp
	src/TilingViewModel.cpp
)
set(TILING_HEADERS
	include/HelperContributionViewModel.hpp
	include/SolutionIntervalViewModel.hpp
	include/TilingTheme.hpp
	include/TilingBackend.hpp
	include/TilingModel.hpp
	include/TilingViewModel.hpp
)
add_executable(
	eagine-msgbus-tiling
	${TILING_RESOURCES}
	${TILING_SOURCES}
	${TILING_HEADERS}
)

eagine_add_exe_analysis(eagine-msgbus-tiling)

target_include_directories(
	eagine-msgbus-tiling
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
	eagine-msgbus-tiling
	PUBLIC
		EAGine::MsgBus
		Qt5::Core
		Qt5::Widgets
		Qt5::Qml
		Qt5::Quick
)


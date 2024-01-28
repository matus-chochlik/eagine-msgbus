# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt

# tiling
set(TILING_RESOURCES
	tiling.qrc
)
set(TILING_SOURCES
	tiling.cpp
	src/PaintedImage.cpp
	src/HelperContributionViewModel.cpp
	src/SolutionIntervalViewModel.cpp
	src/SolutionProgressViewModel.cpp
	src/TilingTheme.cpp
	src/TilingBackend.cpp
	src/TilingModel.cpp
	src/TilingViewModel.cpp
)
set(TILING_HEADERS
	include/PaintedImage.hpp
	include/HelperContributionViewModel.hpp
	include/SolutionIntervalViewModel.hpp
	include/SolutionProgressViewModel.hpp
	include/TilingTheme.hpp
	include/TilingBackend.hpp
	include/TilingModel.hpp
	include/TilingViewModel.hpp
)
add_executable(
	eagine-msgbus-tiling
	EXCLUDE_FROM_ALL
		${TILING_RESOURCES}
		${TILING_SOURCES}
		${TILING_HEADERS}
)

eagine_add_exe_analysis(eagine-msgbus-tiling)

target_compile_options(
	eagine-msgbus-tiling
	PRIVATE $<$<BOOL:${EAGINE_CLANGXX_COMPILER}>:-Wno-unsafe-buffer-usage>
)
target_compile_options(
	eagine-msgbus-tiling
	PRIVATE $<$<BOOL:${EAGINE_CLANGXX_COMPILER}>:-Wno-extra-semi-stmt;-Wno-redundant-parens>
)
target_include_directories(
	eagine-msgbus-tiling
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
)
eagine_target_modules(
	eagine-msgbus-tiling
	eagine.core
	eagine.msgbus
)
target_link_libraries(
	eagine-msgbus-tiling
	PUBLIC
		Qt5::Core
		Qt5::Widgets
		Qt5::Qml
		Qt5::Quick
)


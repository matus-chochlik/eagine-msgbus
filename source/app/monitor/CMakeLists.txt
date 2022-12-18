# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt

# monitor
set(MONITOR_RESOURCES
	monitor.qrc
)
set(MONITOR_SOURCES
	monitor.cpp
	src/PickRoleProxyModel.cpp
	src/MonitorTheme.cpp
	src/MonitorBackend.cpp
	src/TrackerModel.cpp
	src/NodeListViewModel.cpp
	src/MonitorViewModel.cpp
	src/SelectedItemViewModel.cpp
	src/HostViewModel.cpp
	src/HostParameterViewModel.cpp
	src/InstViewModel.cpp
	src/NodeViewModel.cpp
	src/NodeParameterViewModel.cpp
)
set(MONITOR_HEADERS
	include/PickRoleProxyModel.hpp
	include/MonitorTheme.hpp
	include/MonitorBackend.hpp
	include/TrackerModel.hpp
	include/NodeParameterModel.hpp
	include/NodeListViewModel.hpp
	include/MonitorViewModel.hpp
	include/SelectedItemViewModel.hpp
	include/HostViewModel.hpp
	include/HostParameterModel.hpp
	include/HostParameterViewModel.hpp
	include/InstViewModel.hpp
	include/NodeViewModel.hpp
	include/NodeParameterViewModel.hpp
)
add_executable(
	eagine-msgbus-monitor
	${MONITOR_RESOURCES}
	${MONITOR_SOURCES}
	${MONITOR_HEADERS}
)

eagine_add_exe_analysis(eagine-msgbus-monitor)

target_compile_options(
	eagine-msgbus-monitor	
	PRIVATE $<$<BOOL:${EAGINE_CLANGXX_COMPILER}>:-Wno-unsafe-buffer-usage>
)
target_compile_options(
	eagine-msgbus-monitor	
	PRIVATE $<$<BOOL:${EAGINE_CLANGXX_COMPILER}>:-Wno-extra-semi-stmt;-Wno-redundant-parens>
)
target_include_directories(
	eagine-msgbus-monitor
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
)
eagine_target_modules(eagine-msgbus-monitor eagine.core eagine.msgbus)
target_link_libraries(
	eagine-msgbus-monitor
	PUBLIC
		Qt5::Core
		Qt5::Widgets
		Qt5::Qml
		Qt5::Quick
)


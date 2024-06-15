# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt

# sudoku solver helper
add_executable(
	eagine-msgbus-sudoku-tiling
	main.cpp)
eagine_add_exe_analysis(eagine-msgbus-sudoku-tiling)
eagine_target_modules(
	eagine-msgbus-sudoku-tiling
	std
	eagine.core
	eagine.sslplus
	eagine.msgbus)

eagine_embed_target_resources(
	TARGET eagine-msgbus-sudoku-tiling
	RESOURCES
		BoostLcnse "../../../LICENSE_1_0.txt"
		BashCmpltn "resource/eagine-msgbus-sudoku-tiling.bco"
	PACKED
	ENABLE_SEARCH)

install(
	TARGETS eagine-msgbus-sudoku-tiling
	COMPONENT msgbus-apps
	RUNTIME DESTINATION bin)

install(
	FILES "resource/eagine-msgbus-sudoku-tiling.bco"
	COMPONENT msgbus-apps
	RENAME eagine-msgbus-sudoku-tiling
	PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
	DESTINATION share/bash-completion/completions)

install(
	DIRECTORY etc
	COMPONENT msgbus-apps
	DESTINATION /)

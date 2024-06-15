# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt

# pingable
add_executable(
	eagine-msgbus-pingable
	main.cpp)
eagine_add_exe_analysis(eagine-msgbus-pingable)
eagine_target_modules(
	eagine-msgbus-pingable
	std
	eagine.core
	eagine.sslplus
	eagine.msgbus)

eagine_embed_target_resources(
	TARGET eagine-msgbus-pingable
	RESOURCES
		BoostLcnse "../../../LICENSE_1_0.txt"
		BashCmpltn "resource/eagine-msgbus-pingable.bco"
	PACKED
	ENABLE_SEARCH)

install(
	TARGETS eagine-msgbus-pingable
	COMPONENT msgbus-apps
	RUNTIME DESTINATION bin)

install(
	FILES "resource/eagine-msgbus-pingable.bco"
	COMPONENT msgbus-apps
	RENAME eagine-msgbus-pingable
	PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
	DESTINATION share/bash-completion/completions)

install(
	DIRECTORY etc
	COMPONENT msgbus-apps
	DESTINATION /)

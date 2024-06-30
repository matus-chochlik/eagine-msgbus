# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt

# bridge
add_executable(
	eagine-msgbus-bridge
	main.cpp)
eagine_add_exe_analysis(eagine-msgbus-bridge)
eagine_target_modules(
	eagine-msgbus-bridge
	std
	eagine.core
	eagine.sslplus
	eagine.msgbus)

eagine_embed_target_resources(
	TARGET eagine-msgbus-bridge
	RESOURCES
		BashCmpltn "resource/eagine-msgbus-bridge.bco"
		BoostLcnse "../../../LICENSE_1_0.txt"
	PACKED
	ENABLE_SEARCH)

install(
	TARGETS eagine-msgbus-bridge
	COMPONENT msgbus-apps
	RUNTIME DESTINATION bin)

install(
	FILES "resource/eagine-msgbus-bridge.bco"
	COMPONENT msgbus-apps
	RENAME eagine-msgbus-bridge
	PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
	DESTINATION share/bash-completion/completions)


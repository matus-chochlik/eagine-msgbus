# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt

# pinger
add_executable(
	eagine-msgbus-pinger
	main.cpp)
eagine_add_exe_analysis(eagine-msgbus-pinger)
eagine_target_modules(
	eagine-msgbus-pinger
	std
	eagine.core
	eagine.sslplus
	eagine.msgbus)

eagine_embed_target_resources(
	TARGET eagine-msgbus-pinger
	RESOURCES
		BoostLcnse "../../../LICENSE_1_0.txt"
		BashCmpltn "resource/eagine-msgbus-pinger.bco"
	PACKED
	ENABLE_SEARCH)

install(
	TARGETS eagine-msgbus-pinger
	COMPONENT msgbus-apps
	RUNTIME DESTINATION bin)

install(
	FILES "resource/eagine-msgbus-pinger.bco"
	COMPONENT msgbus-apps
	RENAME eagine-msgbus-pinger
	PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
	DESTINATION share/bash-completion/completions)


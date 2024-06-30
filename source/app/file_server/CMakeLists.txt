# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt

# file server
add_executable(
	eagine-msgbus-file-server
	main.cpp)
eagine_add_exe_analysis(eagine-msgbus-file-server)
eagine_target_modules(
	eagine-msgbus-file-server
	std
	eagine.core
	eagine.sslplus
	eagine.msgbus)

eagine_embed_target_resources(
	TARGET eagine-msgbus-file-server
	RESOURCES
		BoostLcnse "../../../LICENSE_1_0.txt"
		BashCmpltn "resource/eagine-msgbus-file-server.bco"
	PACKED
	ENABLE_SEARCH)

install(
	TARGETS eagine-msgbus-file-server
	COMPONENT msgbus-apps
	RUNTIME DESTINATION bin)

install(
	FILES "resource/eagine-msgbus-file-server.bco"
	COMPONENT msgbus-apps
	RENAME eagine-msgbus-file-server
	PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
	DESTINATION share/bash-completion/completions)


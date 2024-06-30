# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt

add_executable(
	eagine-msgbus-resource-get
	main.cpp)
eagine_add_exe_analysis(eagine-msgbus-resource-get)
eagine_target_modules(
	eagine-msgbus-resource-get
	std
	eagine.core
	eagine.sslplus
	eagine.msgbus)

eagine_embed_target_resources(
	TARGET eagine-msgbus-resource-get
	RESOURCES
		BoostLcnse "../../../LICENSE_1_0.txt"
		BashCmpltn "resource/eagine-msgbus-resource-get.bco"
	PACKED
	ENABLE_SEARCH)

install(
	TARGETS eagine-msgbus-resource-get
	COMPONENT msgbus-apps
	RUNTIME DESTINATION bin)

install(
	FILES "resource/eagine-msgbus-resource-get.bco"
	COMPONENT msgbus-apps
	RENAME eagine-msgbus-resource-get
	PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
	DESTINATION share/bash-completion/completions)


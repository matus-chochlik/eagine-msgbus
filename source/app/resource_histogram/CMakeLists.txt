# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt

add_executable(
	eagine-msgbus-resource-histogram
	main.cpp)
eagine_add_exe_analysis(eagine-msgbus-resource-histogram)
eagine_target_modules(
	eagine-msgbus-resource-histogram
	std
	eagine.core
	eagine.sslplus
	eagine.msgbus)

eagine_embed_target_resources(
	TARGET eagine-msgbus-resource-histogram
	RESOURCES
		BoostLcnse "../../../LICENSE_1_0.txt"
		BashCmpltn "resource/eagine-msgbus-resource-histogram.bco"
	PACKED
	ENABLE_SEARCH)

install(
	TARGETS eagine-msgbus-resource-histogram
	COMPONENT msgbus-apps
	RUNTIME DESTINATION bin)

install(
	FILES "resource/eagine-msgbus-resource-histogram.bco"
	COMPONENT msgbus-apps
	RENAME eagine-msgbus-resource-histogram
	PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
	DESTINATION share/bash-completion/completions)


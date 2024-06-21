# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt

# router
add_executable(
	eagine-msgbus-router
	main.cpp)
eagine_add_exe_analysis(eagine-msgbus-router)
eagine_target_modules(
	eagine-msgbus-router
	std
	eagine.core
	eagine.sslplus
	eagine.msgbus)

eagine_embed_target_resources(
	TARGET eagine-msgbus-router
	RESOURCES
		BoostLcnse "../../../LICENSE_1_0.txt"
		BashCmpltn "resource/eagine-msgbus-router.bco"
	PACKED
	ENABLE_SEARCH)

install(
	TARGETS eagine-msgbus-router
	COMPONENT msgbus-apps
	RUNTIME DESTINATION bin)

install(
	FILES "resource/eagine-msgbus-router.bco"
	COMPONENT msgbus-apps
	RENAME eagine-msgbus-router
	PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
	DESTINATION share/bash-completion/completions)

install(
	DIRECTORY etc
	COMPONENT msgbus-apps
	DESTINATION /)

eagine_add_package_conffiles(msgbus-apps etc)


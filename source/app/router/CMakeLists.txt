# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt

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


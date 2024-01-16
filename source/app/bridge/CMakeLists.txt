# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt

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


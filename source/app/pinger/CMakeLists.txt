# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt

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
		BashCmpltn "resource/eagine-msgbus-pinger.bco"
	PACKED
	ENABLE_SEARCH)


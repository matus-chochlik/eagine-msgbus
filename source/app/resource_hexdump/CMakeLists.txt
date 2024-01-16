# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt

add_executable(
	eagine-msgbus-resource-hexdump
	main.cpp)
eagine_add_exe_analysis(eagine-msgbus-resource-hexdump)
eagine_target_modules(
	eagine-msgbus-resource-hexdump
	std
	eagine.core
	eagine.sslplus
	eagine.msgbus)

eagine_embed_target_resources(
	TARGET eagine-msgbus-resource-hexdump
	RESOURCES
		BoostLcnse "../../../LICENSE_1_0.txt"
		BashCmpltn "resource/eagine-msgbus-resource-hexdump.bco"
	PACKED
	ENABLE_SEARCH)


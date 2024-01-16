# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt

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


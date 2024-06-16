# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt
#
add_custom_target(eagine-msgbus-examples ALL)
set_target_properties(
	eagine-msgbus-examples
	PROPERTIES FOLDER "Example/MsgBus"
)

function(eagine_install_example TARGET_NAME)
	install(
		TARGETS ${TARGET_NAME}
		COMPONENT msgbus-examples
		DESTINATION share/eagine/example/msgbus)
endfunction()

function(eagine_example_common EXAMPLE_NAME)
	add_executable(
		eagine-${EXAMPLE_NAME}
		EXCLUDE_FROM_ALL
		"${EXAMPLE_NAME}.cpp"
		${ARGN}
	)
	add_dependencies(eagine-msgbus-examples eagine-${EXAMPLE_NAME})
	eagine_target_modules(
		eagine-${EXAMPLE_NAME}
		std
		eagine.core
		eagine.sslplus
		eagine.msgbus
	)

	set_target_properties(
		eagine-${EXAMPLE_NAME}
		PROPERTIES FOLDER "Example/MsgBus"
	)
	eagine_install_example(eagine-${EXAMPLE_NAME})
endfunction()

eagine_example_common(001_log_certs)
#eagine_example_common(001_signal)
eagine_example_common(001_loopback)
eagine_example_common(002_direct)
eagine_example_common(003_shutdown)
eagine_example_common(004_discovery)
eagine_example_common(005_broadcast_blob)
eagine_example_common(005_byte_histogram)
eagine_example_common(005_topology)
eagine_example_common(006_ping)
eagine_example_common(006_pong)
eagine_example_common(007_ping)
eagine_example_common(007_pong)
eagine_example_common(007_pong_registry)
eagine_example_common(008_pong_registry)
eagine_example_common(008_file_histogram)
eagine_example_common(009_fib_service)
eagine_example_common(010_fib_async)
eagine_example_common(010_fib_futures)
eagine_example_common(011_fibonacci)
eagine_example_common(012_fib_threads)
eagine_example_common(012_sudoku_threads)
eagine_example_common(013_conn_setup)
eagine_example_common(014_tracker)
eagine_example_common(015_stream)
#
eagine_example_common(006_stream_histogram)
eagine_embed_target_resources(
	TARGET eagine-006_stream_histogram
	RESOURCES
		ownSource "${CMAKE_CURRENT_SOURCE_DIR}/006_stream_histogram.cpp"
	PACKED
	ENABLE_SEARCH
)

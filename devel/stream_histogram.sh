#!/bin/bash
# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt
#
install_prefix="$(<$(dirname ${0})/../INSTALL_PREFIX)"
log_args=("--min-log-severity" "stat")
conn_type="--msgbus-asio-local-stream"
#
pids=()
termpids=()
#
${install_prefix}/bin/eagine-msgbus-router \
	"${log_args[@]}" \
	${conn_type} \
	& termpids+=($!)
sleep 2
${install_prefix}/bin/eagine-msgbus-file-server \
	"${log_args[@]}" \
	${conn_type} \
	& termpids+=($!)
sleep 3
${install_prefix}/share/eagine/example/msgbus/eagine-006_stream_histogram \
	"${log_args[@]}" \
	${conn_type} "${@}" \
	& pids+=($!)

for pid in ${pids[@]}
do wait ${pid}
done

kill -TERM ${termpids[@]}

for pid in ${termpids[@]}
do wait ${pid}
done


#!/bin/bash
# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
#
variant=${1:-007}
install_prefix="$(<$(dirname ${0})/../INSTALL_PREFIX)"
log_args=("--min-log-severity" "stat")
conn_type="--msgbus-asio-local-stream"
#
pids=()
#
${install_prefix}/bin/eagine-msgbus-router \
	"${log_args[@]}" \
	${conn_type} \
	--msgbus-router-shutdown-verify false \
	--msgbus-router-shutdown-delay 15s \
	& pids+=($!)
sleep 1
${install_prefix}/share/eagine/example/msgbus/eagine-${variant}_ping \
	"${log_args[@]}" \
	--ping-count ${2:-1M} \
	--ping-batch ${3:-10k} \
	--ping-repeat ${4:-1} \
	${conn_type} \
	& pids+=($!)
sleep 5
${install_prefix}/share/eagine/example/msgbus/eagine-${variant}_pong \
	--pingable-id 1111 \
	"${log_args[@]}" \
	${conn_type} \
	& pids+=($!)

for pid in ${pids[@]}
do wait ${pid}
done


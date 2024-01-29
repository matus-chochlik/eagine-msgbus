#!/bin/bash
# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt
#
install_prefix="$(<$(dirname ${0})/../INSTALL_PREFIX)"
variant=${1:-007}
log_args=("--min-log-severity" "stat")
conn_type="--msgbus-asio-local-stream"
ping_addr="/tmp/ping_reg"
#
pids=()
#
${install_prefix}/bin/eagine-msgbus-router \
	"${log_args[@]}" \
	${conn_type} \
	--msgbus-router-address ${ping_addr} \
	--msgbus-router-id-major 1000 \
	--msgbus-router-id-count 1000 \
	--msgbus-router-shutdown-verify false \
	& pids+=($!)
sleep 1
${install_prefix}/share/eagine/example/msgbus/eagine-007_ping \
	"${log_args[@]}" \
	${conn_type} \
	--msgbus-router-address ${ping_addr} \
	--ping-count ${2:-1M} \
	--ping-batch ${3:-10k} \
	& pids+=($!)
sleep 5
${install_prefix}/share/eagine/example/msgbus/eagine-${variant}_pong_registry \
	"${log_args[@]}" \
	${conn_type} \
	--msgbus-router-address ${ping_addr} \
	--msgbus-router-id-major 2000 \
	--msgbus-router-id-count 1000 \
	--ponger-count ${3:-4} \
	& pids+=($!)

for pid in ${pids[@]}
do wait ${pid}
done


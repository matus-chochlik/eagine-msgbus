#!/bin/bash
# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
#
variant=${1:-007}
install_prefix="$(<$(dirname ${0})/../INSTALL_PREFIX)"
log_args=("--use-asio-log" "--min-log-severity" "stat")
conn_type="--msgbus-posix-mqueue"
ping_addr="eagiping"
pong_addr="eagipong"
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
${install_prefix}/bin/eagine-msgbus-router \
	"${log_args[@]}" \
	${conn_type} \
	--msgbus-router-address ${pong_addr} \
	--msgbus-router-id-major 2000 \
	--msgbus-router-id-count 1000 \
	--msgbus-router-shutdown-verify false \
	& pids+=($!)
sleep 5
${install_prefix}/bin/eagine-local-bridge \
	"${log_args[@]}" \
	${conn_type} \
	-c --msgbus-bridge-shutdown-verify false \
	-l --msgbus-router-address ${ping_addr} \
	-r --msgbus-router-address ${pong_addr} \
	& pids+=($!)
sleep 1
${install_prefix}/share/eagine/example/msgbus/eagine-${variant}_pong \
	"${log_args[@]}" \
	${conn_type} \
	--msgbus-router-address ${pong_addr} \
	& pids+=($!)
sleep 1
${install_prefix}/share/eagine/example/msgbus/eagine-${variant}_ping \
	"${log_args[@]}" \
	--ping-count ${2:-1M} \
	${conn_type} \
	--msgbus-router-address ${ping_addr} \
	& pids+=($!)

for pid in ${pids[@]}
do wait ${pid}
done


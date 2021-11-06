#!/bin/bash
# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
#
variant=${1:-007}
install_prefix="$(<$(dirname ${0})/../INSTALL_PREFIX)"
log_args="--min-log-severity" "stat")
conn_type="--msgbus-asio-local-stream"
ping_addr="/tmp/ping"
pong_addr="/tmp/pong"
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
sleep 1
${install_prefix}/bin/eagine-local-bridge \
	"${log_args[@]}" \
	${conn_type} \
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
	--ping-count ${2:-300k} \
	${conn_type} \
	--msgbus-router-address ${ping_addr} \
	& pids+=($!)
sleep 10
${install_prefix}/share/eagine/example/msgbus/eagine-005_topology \
	"${log_args[@]}" \
	${conn_type} \
	--msgbus-router-address ${pong_addr} > /tmp/eagi-mb-topo.dot \
	& pids+=($!)

for pid in ${pids[@]}
do wait ${pid}
done

#!/bin/bash
# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
#
variant=${1:-007}
install_prefix="$(<$(dirname ${0})/../INSTALL_PREFIX)"
log_args=("--min-log-severity" "stat")
conn_type="--msgbus-asio-tcp-ipv4"
#
pids=()
#
${install_prefix}/bin/eagine-msgbus-router \
	"${log_args[@]}" \
	${conn_type} \
	--msgbus-router-shutdown-verify false \
	& pids+=($!)
sleep 1
${install_prefix}/share/eagine/example/msgbus/eagine-${variant}_ping \
	"${log_args[@]}" \
	--ping-count ${2:-1M} \
	--ping-batch ${3:-10k} \
	${conn_type} \
	& pids+=($!)
sleep 5
${install_prefix}/share/eagine/example/msgbus/eagine-${variant}_pong \
	--pingable-id 2222 \
	"${log_args[@]}" \
	${conn_type} \
	& pids+=($!)

for pid in ${pids[@]}
do wait ${pid}
done


#!/bin/bash
# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt
#
install_prefix="$(<$(dirname ${0})/../INSTALL_PREFIX)"
log_args=("--min-log-severity" "stat")
conn_args=()
case "${1}" in
	posixmq) conn_args+=("--msgbus-posix-mqueue");;
	udpip4) conn_args+=("--msgbus-asio-udp-ipv4");;
	tcpip4) conn_args+=("--msgbus-asio-tcp-ipv4");;
	local|*) conn_args+=("--msgbus-asio-local-stream");;
esac
#
tilings=${3:-2}
pids=()
termpids=()
#
${install_prefix}/bin/eagine-msgbus-router \
	"${log_args[@]}" \
	"${conn_args[@]}" \
	--msgbus-router-shutdown-verify false \
	--msgbus-router-shutdown-delay 15s \
	--msgbus-router-id-major 1 \
	--msgbus-router-id-count 999 \
	& termpids+=($!)
sleep 1
${install_prefix}/bin/eagine-msgbus-sudoku-helper \
	"${log_args[@]}" \
	"${conn_args[@]}" \
	--msgbus-router-id-major 1000 \
	--msgbus-router-id-count 100 \
	& termpids+=($!)
sleep 5
for t in $(seq 1 ${tilings})
do
	${install_prefix}/bin/eagine-msgbus-tiling \
		"${log_args[@]}" \
		"${conn_args[@]}" \
		--msgbus-sudoku-solver-width ${2:-64} \
		--msgbus-sudoku-solver-height ${2:-64} \
		--msgbus-sudoku-solver-output-path /tmp/tiling${t}.txt \
		& pids+=($!)
	sleep 2
done

for pid in ${pids[@]}
do wait ${pid}
done

kill -TERM ${termpids[@]}

for pid in ${termpids[@]}
do wait ${pid}
done


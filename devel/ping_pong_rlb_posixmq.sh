#!/bin/bash
# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt
#
"$(dirname ${0})/ping_pong_rlb.sh" \
	-S "variant=${1:-007}" \
	-S "ping_count=${2:-1M}" \
	-S "ping_batch=${3:-10k}" \
	-S "ping_repeat=${4:-1}" \
	-S "conn_type=posixmq"

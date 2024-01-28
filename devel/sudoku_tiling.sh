#!/bin/bash
# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt
#
"$(dirname ${0})/procman.sh" \
	-S "conn_type=${1:-local}" \
	-S "tile_size=${2:-64}" \
	-I "solver=${3:-2}" \
	-P "solver=${4:-2}" \
	"$(dirname ${0})/sudoku_tiling.eagiproc"

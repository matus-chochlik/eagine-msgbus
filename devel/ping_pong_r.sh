#!/bin/bash
# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
#
"$(dirname ${0})/procman.sh" "${@}" "$(dirname ${0})/ping_pong_r.eagiproc"

#!/bin/bash
# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
#
if [[ -x "$(dirname ${0})/../../eagine-core/source/app/procman.py" ]]
then procman="$(dirname ${0})/../../eagine-core/source/app/procman.py"
elif [[ -x "$(dirname ${0})/../submodules/eagine-core/source/app/procman.py" ]]
then procman="$(dirname ${0})/../submodules/eagine-core/source/app/procman.py"
else procman="$(<$(dirname ${0})/../INSTALL_PREFIX)/bin/eagine-procman"
fi

"${procman}" "${@}" -n -

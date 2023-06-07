#!/bin/bash
if [[ -x "$(dirname ${0})/../../eagine-core/source/app/procman.py" ]]
then procman="$(dirname ${0})/../../eagine-core/source/app/procman.py"
elif [[ -x "$(dirname ${0})/../submodules/eagine-core/source/app/procman.py" ]]
then procman="$(dirname ${0})/../submodules/eagine-core/source/app/procman.py"
else procman="$(<$(dirname ${0})/../INSTALL_PREFIX)/bin/eagine-procman"
fi

"${procman}" "${@}" -l -

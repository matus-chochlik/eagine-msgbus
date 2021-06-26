# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
#
find_program(PDFLATEX_COMMAND pdflatex)
find_program(PYTHON3_COMMAND python3)

if(PDFLATED_COMMAND AND PYTHON3_COMMAND)
	add_subdirectory(latex)
endif()

# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt
#

add_custom_target(eagine-msgbus-latex-doc)
add_dependencies(eagine-msgbus-doc eagine-msgbus-latex-doc)

file(GENERATE
    OUTPUT "beamercolorthemeeagine.sty"
    INPUT "beamercolorthemeeagine.sty.in"
)

file(GENERATE
    OUTPUT "beamer_eagine.tex"
    INPUT "beamer_eagine.tex.in"
)

function(eagine_add_latex_presentation)
    if(PDFLATEX_COMMAND)
        set(SRC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${SRC_NAME}.tex")

        add_custom_command(
            OUTPUT "${SRC_NAME}.tex"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SRC_PATH}" "./"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            DEPENDS ${SRC_PATH}
            COMMENT "Copying ${SRC_NAME} LaTeX source"
        )

        set(PRESENTATION_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/beamercolorthemeeagine.sty"
            "${CMAKE_CURRENT_BINARY_DIR}/beamer_eagine.tex"
            "${CMAKE_CURRENT_BINARY_DIR}/${SRC_NAME}.tex"
        )

        add_custom_target(
            ${SRC_NAME}-latex-sources
            DEPENDS ${PRESENTATION_DEPENDS}
        )

        foreach(DEPNAME ${ARGN})
            list(APPEND PRESENTATION_DEPENDS eagine-${DEPNAME}.pdf)
        endforeach()

        add_custom_command(
            OUTPUT eagine-${SRC_NAME}.toc
            COMMAND pdflatex
                "-shell-escape"
                "-interaction" "batchmode"
                "-halt-on-error"
                "-output-directory=${CMAKE_CURRENT_BINARY_DIR}"
                "-jobname" "eagine-${SRC_NAME}"
                "${SRC_NAME}.tex"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            DEPENDS
                ${PRESENTATION_DEPENDS}
                ${SRC_NAME}-latex-sources
            COMMENT "Preparing eagine-${SRC_NAME}.pdf"
        )

        add_custom_command(
            OUTPUT eagine-${SRC_NAME}.pdf
            COMMAND pdflatex
                "-shell-escape"
                "-interaction" "batchmode"
                "-output-directory=${CMAKE_CURRENT_BINARY_DIR}"

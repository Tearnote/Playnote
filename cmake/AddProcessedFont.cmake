# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
# or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
# or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
# or distributed except according to those terms.

include_guard()

include(cmake/SetupPythonEnv.cmake)

function(add_processed_font)
	cmake_parse_arguments(ARG "" "INPUT;OUTPUT" "" ${ARGN})
	if(NOT ARG_INPUT OR NOT ARG_OUTPUT)
		message(FATAL_ERROR "Usage: add_processed_font(INPUT <file> OUTPUT <file>)")
	endif()

	get_filename_component(OUTPUT_DIR ${ARG_OUTPUT} DIRECTORY)
	set(PROCESS_FONT_SCRIPT "${PROJECT_SOURCE_DIR}/tools/process_font.py")

	add_custom_command(
		OUTPUT ${ARG_OUTPUT}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
		COMMAND ${PYTHON_VENV_EXE} ${PROCESS_FONT_SCRIPT} ${ARG_INPUT} ${ARG_OUTPUT}
		DEPENDS ${ARG_INPUT} ${PROCESS_FONT_SCRIPT} ${PYTHON_VENV_STAMP}
		COMMENT "Processing font ${ARG_INPUT}"
		VERBATIM
	)
endfunction()

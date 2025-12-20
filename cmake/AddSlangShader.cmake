# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
# or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
# or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
# or distributed except according to those terms.

include_guard()

include(cmake/FetchSlang.cmake)

function(add_slang_shader)
	cmake_parse_arguments(ARG "" "INPUT;OUTPUT" "" ${ARGN})
	if(NOT ARG_INPUT OR NOT ARG_OUTPUT)
		message(FATAL_ERROR "Usage: add_slang_shader(INPUT <file> OUTPUT <file>)")
	endif()

	get_filename_component(SHADER_NAME ${ARG_INPUT} NAME_WLE)
	string(MAKE_C_IDENTIFIER "${SHADER_NAME}" SHADER_SYMBOL)
	get_filename_component(OUTPUT_DIR ${ARG_OUTPUT} DIRECTORY)

	add_custom_command(
		OUTPUT ${ARG_OUTPUT}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
		COMMAND ${SLANGC_EXECUTABLE}
			-target spirv
			-capability vk_mem_model
			-fvk-use-entrypoint-name
			-source-embed-style u32
			-source-embed-name ${SHADER_SYMBOL}_spv
			$<$<CONFIG:Debug,RelWithDebInfo>:-g>
			-O2
			-depfile ${ARG_OUTPUT}.d
			-o ${ARG_OUTPUT}
			${ARG_INPUT}
		DEPENDS ${ARG_INPUT}
		DEPFILE ${ARG_OUTPUT}.d
		COMMENT "Compiling Slang shader ${ARG_INPUT}"
		VERBATIM COMMAND_EXPAND_LISTS
	)
endfunction()

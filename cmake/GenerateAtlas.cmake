# Copyright (c) 2026 Tearnote (Hubert Maraszek)
#
# Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
# or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
# or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
# or distributed except according to those terms.

include_guard()

include(cmake/Dependencies.cmake)

add_executable(GenerateAtlas
	src/lib/harfbuzz.cpp
	src/lib/msdf.cpp
	src/lib/icu.cpp
	src/io/file.cpp
	src/gfx/text.cpp
	src/utils/logger.cpp
	tools/generate_atlas.cpp
)
target_link_libraries(GenerateAtlas
	PRIVATE readerwriterqueue::readerwriterqueue
	PRIVATE concurrentqueue::concurrentqueue
	PRIVATE msdf-atlas-gen::msdf-atlas-gen
	PRIVATE magic_enum::magic_enum
	PRIVATE libassert::assert
	PRIVATE Freetype::Freetype
	PRIVATE harfbuzz::harfbuzz
	PRIVATE libcoro
	PRIVATE Boost::container
	PRIVATE Boost::boost
	PRIVATE quill::quill
	PRIVATE ICU::i18n
	PRIVATE ICU::uc
)
target_include_directories(Playnote
	PRIVATE ${ZPP_BITS_INCLUDE_DIRS}
)
target_include_directories(GenerateAtlas PRIVATE src)

function(generate_atlas)
	cmake_parse_arguments(ARG "" "OUTPUT" "FONTS" ${ARGN})
	if(NOT ARG_OUTPUT OR NOT ARG_FONTS)
		message(FATAL_ERROR "Usage: generate_atlas(OUTPUT <file> FONTS <fonts...>)")
	endif()

	get_filename_component(OUTPUT_DIR ${ARG_OUTPUT} DIRECTORY)

	# Resolve inputs to absolute paths
	set(ABS_INPUTS)
	foreach(INPUT IN LISTS ARG_FONTS)
		get_filename_component(ABS_INPUT "${INPUT}" ABSOLUTE)
		list(APPEND ABS_INPUTS "${ABS_INPUT}")
	endforeach()

	add_custom_command(
		OUTPUT ${ARG_OUTPUT}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
		COMMAND GenerateAtlas ${ARG_OUTPUT} ${ABS_INPUTS}
		DEPENDS ${ARG_FONTS} $<TARGET_FILE:GenerateAtlas>
		COMMENT "Generating font atlas cache at ${ARG_OUTPUT}"
		VERBATIM
	)
endfunction()

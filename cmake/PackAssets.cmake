# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
# or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
# or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
# or distributed except according to those terms.

include_guard()

include(cmake/Dependencies.cmake)

add_executable(PackAssets
	src/lib/sqlite.cpp
	src/io/file.cpp
	tools/pack_assets.cpp
)
target_link_libraries(PackAssets
	PRIVATE readerwriterqueue::readerwriterqueue
	PRIVATE concurrentqueue::concurrentqueue
	PRIVATE unofficial::sqlite3::sqlite3
	PRIVATE magic_enum::magic_enum
	PRIVATE libassert::assert
	PRIVATE libcoro
	PRIVATE Boost::container
	PRIVATE Boost::boost
)
target_include_directories(PackAssets PRIVATE src)

function(pack_assets)
	cmake_parse_arguments(ARG "" "OUTPUT" "INPUTS" ${ARGN})
	if(NOT ARG_OUTPUT OR NOT ARG_INPUTS)
		message(FATAL_ERROR "Usage: pack_assets(OUTPUT <db_file> INPUTS <assets...>)")
	endif()

	get_filename_component(OUTPUT_DIR ${ARG_OUTPUT} DIRECTORY)

	# Resolve inputs to absolute paths
	set(ABS_INPUTS)
	foreach(INPUT IN LISTS ARG_INPUTS)
		get_filename_component(ABS_INPUT "${INPUT}" ABSOLUTE)
		list(APPEND ABS_INPUTS "${ABS_INPUT}")
	endforeach()

	add_custom_command(
		OUTPUT ${ARG_OUTPUT}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
		COMMAND PackAssets ${ARG_OUTPUT} ${ABS_INPUTS}
		DEPENDS ${ARG_INPUTS} $<TARGET_FILE:PackAssets>
		COMMENT "Packing assets into ${ARG_OUTPUT}"
		VERBATIM
	)
endfunction()

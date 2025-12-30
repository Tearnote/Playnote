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
	src/lib/zstd.cpp
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
	PRIVATE zstd::libzstd
)
target_include_directories(PackAssets PRIVATE src)

function(pack_assets)
	cmake_parse_arguments(ARG "" "OUTPUT" "RAW;COMPRESS" ${ARGN})
	if(NOT ARG_OUTPUT OR (NOT ARG_RAW AND NOT ARG_COMPRESS))
		message(FATAL_ERROR "Usage: pack_assets(OUTPUT <db_file> [RAW <assets...>] [COMPRESS <assets...>])")
	endif()

	get_filename_component(OUTPUT_DIR ${ARG_OUTPUT} DIRECTORY)

	# Resolve inputs to absolute paths
	foreach(INPUT IN LISTS ARG_RAW)
		get_filename_component(ABS_INPUT "${INPUT}" ABSOLUTE)
		list(APPEND ALL_INPUTS "${ABS_INPUT}")
		list(APPEND DEPENDS_LIST "${ABS_INPUT}")
	endforeach()
	foreach(INPUT IN LISTS ARG_COMPRESS)
		get_filename_component(ABS_INPUT "${INPUT}" ABSOLUTE)
		list(APPEND ALL_INPUTS "${ABS_INPUT}:z")
		list(APPEND DEPENDS_LIST "${ABS_INPUT}")
	endforeach()

	add_custom_command(
		OUTPUT ${ARG_OUTPUT}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
		COMMAND PackAssets ${ARG_OUTPUT} ${ALL_INPUTS}
		DEPENDS ${DEPENDS_LIST} $<TARGET_FILE:PackAssets>
		COMMENT "Packing assets into ${ARG_OUTPUT}"
		VERBATIM
	)
endfunction()

# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
# or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
# or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
# or distributed except according to those terms.

include_guard()

include(cmake/AddSlangShader.cmake)

set(PLAYNOTE_SHADER_PREFIX src/gpu)
set(PLAYNOTE_SHADER_SOURCES
	worklist_gen.slang
	worklist_sort.slang
	draw_all.slang
	imgui.slang
)

set(ASSET_PATHS
	assets/unifont-16.0.03.ttf
	assets/Mplus2.ttf
	assets/Pretendard.ttf
)

# Compile shaders
foreach(SHADER_PATH ${PLAYNOTE_SHADER_SOURCES})
	set(INPUT_PATH ${PROJECT_SOURCE_DIR}/${PLAYNOTE_SHADER_PREFIX}/${SHADER_PATH})
	set(OUTPUT_PATH ${PROJECT_BINARY_DIR}/$<CONFIG>/generated/spv/${SHADER_PATH}.spv.h)
	add_slang_shader(INPUT ${INPUT_PATH} OUTPUT ${OUTPUT_PATH})
	list(APPEND PLAYNOTE_SHADER_OUTPUTS ${OUTPUT_PATH})
endforeach()

foreach(ASSET_PATH ${PLAYNOTE_ASSET_PATHS})
	set(ASSET_OUTPUT ${PROJECT_BINARY_DIR}/$<CONFIG>/assets/${ASSET_PATH})
	add_custom_command(
		OUTPUT ${ASSET_OUTPUT}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/$<CONFIG>/assets
		COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/${ASSET_PATH} ${ASSET_OUTPUT}
		DEPENDS ${ASSET_PATH}
		VERBATIM COMMAND_EXPAND_LISTS
	)
	list(APPEND PLAYNOTE_ASSET_OUTPUTS ${PLAYNOTE_ASSET_OUTPUT})
endforeach()

add_custom_target(PlaynoteAssets DEPENDS ${PLAYNOTE_SHADER_OUTPUTS} ${PLAYNOTE_ASSET_OUTPUTS})

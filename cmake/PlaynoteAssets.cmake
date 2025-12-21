# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
# or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
# or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
# or distributed except according to those terms.

include_guard()

include(cmake/AddProcessedFont.cmake)
include(cmake/AddSlangShader.cmake)
include(cmake/GenerateAtlas.cmake)
include(cmake/PackAssets.cmake)

set(PLAYNOTE_SHADER_PREFIX src/gpu)
set(PLAYNOTE_SHADER_SOURCES
	worklist_gen.slang
	worklist_sort.slang
	draw_all.slang
	imgui.slang
)

set(PLAYNOTE_FONT_PREFIX fonts)
set(PLAYNOTE_FONTS
	Mplus2-Medium.ttf
	Pretendard-Medium.ttf
)

# Compile shaders
foreach(SHADER_PATH ${PLAYNOTE_SHADER_SOURCES})
	set(INPUT_PATH ${PROJECT_SOURCE_DIR}/${PLAYNOTE_SHADER_PREFIX}/${SHADER_PATH})
	set(OUTPUT_PATH ${PROJECT_BINARY_DIR}/$<CONFIG>/generated/spv/${SHADER_PATH}.spv.h)
	add_slang_shader(INPUT ${INPUT_PATH} OUTPUT ${OUTPUT_PATH})
	list(APPEND PLAYNOTE_SHADER_OUTPUTS ${OUTPUT_PATH})
endforeach()

# Process fonts
foreach(FONT_PATH ${PLAYNOTE_FONTS})
	set(INPUT_PATH ${PROJECT_SOURCE_DIR}/${PLAYNOTE_FONT_PREFIX}/${FONT_PATH})
	set(OUTPUT_PATH ${PROJECT_BINARY_DIR}/$<CONFIG>/generated/ttf/${FONT_PATH})
	add_processed_font(INPUT ${INPUT_PATH} OUTPUT ${OUTPUT_PATH})
	list(APPEND PLAYNOTE_FONT_OUTPUTS ${OUTPUT_PATH})
endforeach()

# Build the initial font atlas
set(PLAYNOTE_FONT_ATLAS_BITMAP ${PROJECT_BINARY_DIR}/$<CONFIG>/generated/font_atlas_bitmap.bin)
set(PLAYNOTE_FONT_ATLAS_LAYOUT ${PROJECT_BINARY_DIR}/$<CONFIG>/generated/font_atlas_layout.bin)
generate_atlas(BITMAP ${PLAYNOTE_FONT_ATLAS_BITMAP} LAYOUT ${PLAYNOTE_FONT_ATLAS_LAYOUT} FONTS
	${PLAYNOTE_FONT_OUTPUTS})

# Pack into an asset database
set(PLAYNOTE_ASSET_DB ${PROJECT_BINARY_DIR}/$<CONFIG>/assets.db)
pack_assets(OUTPUT ${PLAYNOTE_ASSET_DB} INPUTS
	${PLAYNOTE_FONT_OUTPUTS}
	${PLAYNOTE_FONT_ATLAS_BITMAP} ${PLAYNOTE_FONT_ATLAS_LAYOUT}
	${PROJECT_SOURCE_DIR}/fonts/unifont-16.0.03.ttf
)

add_custom_target(PlaynoteAssets DEPENDS ${PLAYNOTE_SHADER_OUTPUTS} ${PLAYNOTE_ASSET_DB})

# This software is dual-licensed. For more details, please consult LICENSE.txt.
# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# cmake/Assets.cmake:
# Target that copies a list of files into the build.

include_guard()

set(ASSET_DIR_PREFIX assets/)
set(ASSET_PATHS
	unifont-16.0.03.ttf
)

foreach(ASSET_PATH ${ASSET_PATHS})
	set(ASSET_OUTPUT ${PROJECT_BINARY_DIR}/$<CONFIG>/assets/${ASSET_PATH})
	add_custom_command(
		OUTPUT ${ASSET_OUTPUT}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/$<CONFIG>/assets
		COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/${ASSET_DIR_PREFIX}${ASSET_PATH} ${ASSET_OUTPUT}
		DEPENDS ${ASSET_DIR_PREFIX}${ASSET_PATH}
		VERBATIM COMMAND_EXPAND_LISTS
	)
	list(APPEND ASSET_OUTPUTS ${ASSET_OUTPUT})
endforeach()

add_custom_target(Playnote_Assets DEPENDS ${ASSET_OUTPUTS})

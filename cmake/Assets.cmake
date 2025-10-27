# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
# or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
# or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
# or distributed except according to those terms.

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

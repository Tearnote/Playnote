# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
# or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
# or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
# or distributed except according to those terms.

include_guard()

include(cmake/Dependencies.cmake)
include(cmake/FetchSlang.cmake)

set(SHADER_DIR_PREFIX src/gpu/)
set(SHADER_SOURCES
	circles.slang
	gamma.slang
	rects.slang
	imgui.slang
)

if(WIN32)
	find_program(GLSLC glslc REQUIRED)
else()
	set(GLSLC ${Vulkan_GLSLC_EXECUTABLE})
endif()

foreach(SHADER_PATH ${SHADER_SOURCES})
	get_filename_component(SHADER_DIR ${SHADER_PATH} DIRECTORY)
	get_filename_component(SHADER_ID ${SHADER_PATH} NAME_WLE)
	string(REPLACE "/" "_" SHADER_ID ${SHADER_ID})
	string(REPLACE "." "_" SHADER_ID ${SHADER_ID})
	set(SHADER_OUTPUT ${PROJECT_BINARY_DIR}/$<CONFIG>/generated/spv/${SHADER_PATH}.spv.h)
	add_custom_command(
		OUTPUT ${SHADER_OUTPUT}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/$<CONFIG>/generated/spv/${SHADER_DIR}
		COMMAND ${SLANGC_EXECUTABLE} -target spirv -capability vk_mem_model -fvk-use-entrypoint-name -source-embed-style u32 -source-embed-name ${SHADER_ID}_spv $<$<CONFIG:Debug,RelWithDebInfo>:-g> -depfile ${SHADER_OUTPUT}.d -o ${SHADER_OUTPUT} ${PROJECT_SOURCE_DIR}/${SHADER_DIR_PREFIX}${SHADER_PATH}
		DEPENDS ${SHADER_DIR_PREFIX}${SHADER_PATH}
		DEPFILE ${SHADER_OUTPUT}.d
		VERBATIM COMMAND_EXPAND_LISTS
	)
	list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
endforeach()

add_custom_target(Playnote_Shaders DEPENDS ${SHADER_OUTPUTS})

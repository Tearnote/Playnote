# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
# or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
# or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
# or distributed except according to those terms.

include_guard()

include(cmake/Dependencies.cmake)

set(SHADER_DIR_PREFIX src/gpu/)
set(SHADER_SOURCES
	circles.comp
	gamma.comp
	imgui.vert
	imgui.frag
	rects.vert
	rects.frag
)

if(WIN32)
	find_program(GLSLC glslc REQUIRED)
else()
	set(GLSLC ${Vulkan_GLSLC_EXECUTABLE})
endif()

foreach(SHADER_PATH ${SHADER_SOURCES})
	get_filename_component(SHADER_DIR ${SHADER_PATH} DIRECTORY)
	set(SHADER_OUTPUT ${PROJECT_BINARY_DIR}/$<CONFIG>/generated/spv/${SHADER_PATH}.spv)
	add_custom_command(
		OUTPUT ${SHADER_OUTPUT}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/$<CONFIG>/generated/spv/${SHADER_DIR}
		COMMAND ${GLSLC} --target-env=vulkan1.3 -mfmt=num $<$<CONFIG:Debug,RelWithDebInfo>:-g> -MD -MF ${SHADER_OUTPUT}.d -o ${SHADER_OUTPUT} ${PROJECT_SOURCE_DIR}/${SHADER_DIR_PREFIX}${SHADER_PATH}
		DEPENDS ${SHADER_DIR_PREFIX}${SHADER_PATH}
		DEPFILE ${SHADER_OUTPUT}.d
		VERBATIM COMMAND_EXPAND_LISTS
	)
	list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
endforeach()

add_custom_target(Playnote_Shaders DEPENDS ${SHADER_OUTPUTS})

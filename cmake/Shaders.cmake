# This software is dual-licensed. For more details, please consult LICENSE.txt.
# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# cmake/Shaders.cmake:
# Target that compiles a list of shaders into SPIR-V as a list of 32-bit integer literals.

include_guard()

include(cmake/Dependencies.cmake)

set(SHADER_DIR_PREFIX src/gpu/)
set(SHADER_SOURCES
	imgui.vert
	imgui.frag
	rects.vert
	rects.frag
)

foreach(SHADER_PATH ${SHADER_SOURCES})
	get_filename_component(SHADER_DIR ${SHADER_PATH} DIRECTORY)
	set(SHADER_OUTPUT ${PROJECT_BINARY_DIR}/$<CONFIG>/generated/spv/${SHADER_PATH}.spv)
	add_custom_command(
		OUTPUT ${SHADER_OUTPUT}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/$<CONFIG>/generated/spv/${SHADER_DIR}
		COMMAND Vulkan::glslc --target-env=vulkan1.2 -mfmt=num $<$<CONFIG:Debug,RelWithDebInfo>:-g> -MD -MF ${SHADER_OUTPUT}.d -o ${SHADER_OUTPUT} ${PROJECT_SOURCE_DIR}/${SHADER_DIR_PREFIX}${SHADER_PATH}
		DEPENDS ${SHADER_DIR_PREFIX}${SHADER_PATH}
		DEPFILE ${SHADER_OUTPUT}.d
		VERBATIM COMMAND_EXPAND_LISTS
	)
	list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
endforeach()

add_custom_target(Playnote_Shaders DEPENDS ${SHADER_OUTPUTS})

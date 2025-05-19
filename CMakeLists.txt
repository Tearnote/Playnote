# This software is dual-licensed. For more details, please consult LICENSE.txt.
# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# CMakeLists.txt:
# Declares project files and build configuration.

# Project configuration

cmake_minimum_required(VERSION 3.30)
project(Playnote
	VERSION 0.0.1
	DESCRIPTION "A BMS player hobby project"
	LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Global build settings

set(BUILD_SHARED_LIBS OFF) # Only build static libs
#TODO Compiler and linker flags:
#TODO - nonstandard speedups (fastmath etc)
#TODO - enable exceptions and RTTI
#TODO - safety checks for DEBUG
#TODO - optimizations for RELEASE
#TODO - enable LTO and dead code removal
# Communicate build type to the project
add_compile_definitions("BUILD_TYPE=$<$<CONFIG:Debug>:BUILD_DEBUG>$<$<CONFIG:RelWithDebInfo>:BUILD_RELDEB>$<$<CONFIG:Release>:BUILD_RELEASE>")

include(cmake/Dependencies.cmake)

# Main target

add_executable(Playnote
	src/util/log_macros.hpp
	src/sys/gpu.cpp
	src/gfx/renderer.cpp
	src/config.hpp
	src/main.cpp
)
target_sources(Playnote PUBLIC
	FILE_SET CXX_MODULES FILES
	src/stx/callable.cppm
	src/stx/concepts.cppm
	src/stx/except.cppm
	src/stx/types.cppm
	src/stx/math.cppm
	src/util/charset.cppm
	src/util/logger.cppm
	src/util/raii.cppm
	src/util/file.cppm
	src/sys/window.cppm
	src/sys/audio.cppm
	src/sys/gpu.cppm
	src/sys/os.cppm
	src/gfx/renderer.cppm
	src/gfx/imgui.cppm
	src/bms/parser.cppm
	src/bms/ir.cppm
	src/render_thread.cppm
	src/audio_thread.cppm
	src/input_thread.cppm
	src/globals.cppm
)

target_include_directories(Playnote PRIVATE src) # All includes start from src as root

target_link_libraries(Playnote PRIVATE unordered_dense::unordered_dense)
target_link_libraries(Playnote PRIVATE vk-bootstrap)
target_link_libraries(Playnote PRIVATE TracyClient)
target_link_libraries(Playnote PRIVATE PkgConfig::libsamplerate)
target_link_libraries(Playnote PRIVATE PkgConfig::libsndfile)
target_link_libraries(Playnote PRIVATE PkgConfig::PipeWire)
target_link_libraries(Playnote PRIVATE libassert::assert)
target_link_libraries(Playnote PRIVATE OpenSSL::Crypto)
target_link_libraries(Playnote PRIVATE Boost::boost)
target_link_libraries(Playnote PRIVATE quill)
target_link_libraries(Playnote PRIVATE glfw)
target_link_libraries(Playnote PRIVATE volk)
target_link_libraries(Playnote PRIVATE ICU::i18n)
target_link_libraries(Playnote PRIVATE ICU::uc)
target_link_libraries(Playnote PRIVATE mio::mio)
target_link_libraries(Playnote PRIVATE vuk-extra-imgui-platform)
target_link_libraries(Playnote PRIVATE vuk)

# Assets and other artifacts

include(cmake/Shaders.cmake)
add_dependencies(Playnote Playnote_Shaders)
target_include_directories(Playnote PUBLIC ${PROJECT_BINARY_DIR}/$<CONFIG>/generated)
include(cmake/Assets.cmake)
add_dependencies(Playnote Playnote_Assets)

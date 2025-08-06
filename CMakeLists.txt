# This software is dual-licensed. For more details, please consult LICENSE.txt.
# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# CMakeLists.txt:
# Declares project files and build configuration.

# Project configuration

cmake_minimum_required(VERSION 3.30)
project(Playnote
	VERSION 0.0.2
	DESCRIPTION "A BMS player hobby project"
	LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Environment check

if(NOT UNIX)
	message(FATAL_ERROR "Unsupported platform; Playnote requires Linux")
endif()
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	message(FATAL_ERROR "Unsupported compiler; Playnote requires Clang")
endif()

# Global build settings

set(BUILD_SHARED_LIBS OFF)
add_compile_options("-Wall;-Wextra;-Wno-deprecated-literal-operator;-Wno-unqualified-std-cast-call;-Wno-missing-field-initializers;-Wno-missing-designated-field-initializers;-Wno-nan-infinity-disabled")
add_compile_options("-march=x86-64-v3;-ffast-math;-fno-strict-float-cast-overflow;-fassume-nothrow-exception-dtor")
add_compile_options("-fno-strict-aliasing;-fexceptions;-frtti")
add_compile_options("$<$<NOT:$<CONFIG:Release>>:-g;-glldb>")
add_compile_options("$<$<CONFIG:Release>:-g0;-flto>;$<$<NOT:$<CONFIG:Debug>>:-O3>") # CMake says LTO isn't supported, but it works?
add_link_options("-fuse-ld=lld;$<$<NOT:$<CONFIG:Debug>>:-O3>;$<$<CONFIG:Release>:-flto>")
# Communicate build type to the project
add_compile_definitions("BUILD_TYPE=$<$<CONFIG:Debug>:BUILD_DEBUG>$<$<CONFIG:RelWithDebInfo>:BUILD_RELDEB>$<$<CONFIG:Release>:BUILD_RELEASE>")

include(cmake/Dependencies.cmake)

# Main target

add_executable(Playnote
	src/main.cpp
)
target_sources(Playnote PUBLIC
	FILE_SET CXX_MODULES FILES
	src/lib/pipewire.cppm
	src/lib/ebur128.cppm
	src/lib/openssl.cppm
	src/lib/ffmpeg.cppm
	src/lib/thread.cppm
	src/lib/vulkan.cppm
	src/lib/imgui.cppm
	src/lib/tracy.cppm
	src/lib/debug.cppm
	src/lib/glfw.cppm
	src/lib/icu.cppm
	src/lib/mio.cppm
	src/dev/window.cppm
	src/dev/audio.cppm
	src/dev/gpu.cppm
	src/dev/os.cppm
	src/gfx/playfield.cppm
	src/gfx/imgui.cppm
	src/bms/audio_player.cppm
	src/bms/cursor.cppm
	src/bms/parser.cppm
	src/bms/chart.cppm
	src/bms/build.cppm
	src/bms/ir.cppm
	src/threads/render_shouts.cppm
	src/threads/audio_shouts.cppm
	src/threads/input_shouts.cppm
	src/threads/broadcaster.cppm
	src/threads/render.cppm
	src/threads/audio.cppm
	src/threads/input.cppm
)

target_include_directories(Playnote PRIVATE src) # All includes start from src as root

target_link_libraries(Playnote PRIVATE vk-bootstrap)
target_link_libraries(Playnote PRIVATE TracyClient)
target_link_libraries(Playnote PRIVATE msd_channel)
target_link_libraries(Playnote PRIVATE PkgConfig::libswresample)
target_link_libraries(Playnote PRIVATE PkgConfig::libavformat)
target_link_libraries(Playnote PRIVATE PkgConfig::libavcodec)
target_link_libraries(Playnote PRIVATE PkgConfig::libavutil)
target_link_libraries(Playnote PRIVATE PkgConfig::PipeWire)
target_link_libraries(Playnote PRIVATE libassert::assert)
target_link_libraries(Playnote PRIVATE OpenSSL::Crypto)
target_link_libraries(Playnote PRIVATE ebur128)
target_link_libraries(Playnote PRIVATE implot)
target_link_libraries(Playnote PRIVATE Boost::container)
target_link_libraries(Playnote PRIVATE Boost::boost)
target_link_libraries(Playnote PRIVATE imgui)
target_link_libraries(Playnote PRIVATE quill)
target_link_libraries(Playnote PRIVATE glfw)
target_link_libraries(Playnote PRIVATE volk)
target_link_libraries(Playnote PRIVATE ICU::i18n)
target_link_libraries(Playnote PRIVATE ICU::uc)
target_link_libraries(Playnote PRIVATE mio::mio)
target_link_libraries(Playnote PRIVATE vuk)
target_compile_definitions(imgui PUBLIC "IMGUI_USER_CONFIG=\"${PROJECT_SOURCE_DIR}/src/lib/imconfig.h\"")

# Assets and other artifacts

include(cmake/Shaders.cmake)
add_dependencies(Playnote Playnote_Shaders)
target_include_directories(Playnote PUBLIC ${PROJECT_BINARY_DIR}/$<CONFIG>/generated)
include(cmake/Assets.cmake)
add_dependencies(Playnote Playnote_Assets)

# This software is dual-licensed. For more details, please consult LICENSE.txt.
# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# CMakeLists.txt:
# Declares project files and build configuration.

# Project configuration

cmake_minimum_required(VERSION 3.30)
project(Playnote
	VERSION 0.0.3
	DESCRIPTION "A BMS player hobby project"
	LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Global build settings

set(BUILD_SHARED_LIBS OFF)
add_compile_options("-Wall;-Wextra")
add_compile_options("-march=x86-64-v3;-ffast-math")
add_compile_options("-fno-strict-aliasing;$<$<COMPILE_LANGUAGE:CXX>:-fexceptions;-frtti>")
add_compile_options("$<$<NOT:$<CONFIG:Release>>:-g>")
add_compile_options("$<$<CONFIG:Release>:-g0>;$<$<NOT:$<CONFIG:Debug>>:-O3>")
add_link_options("$<$<NOT:$<CONFIG:Debug>>:-O3>")
# Communicate build type to the project
add_compile_definitions("BUILD_TYPE=$<$<CONFIG:Debug>:BUILD_DEBUG>$<$<CONFIG:RelWithDebInfo>:BUILD_RELDEB>$<$<CONFIG:Release>:BUILD_RELEASE>")

include(cmake/Dependencies.cmake)

# Main target

add_executable(Playnote
	src/lib/ebur128.cpp
	src/lib/openssl.cpp
	src/lib/ffmpeg.cpp
	src/lib/vulkan.cpp
	src/lib/thread.cpp
	src/lib/debug.cpp
	src/lib/imgui.cpp
	src/lib/glfw.cpp
	src/lib/icu.cpp
	src/lib/vuk.cpp
	src/threads/render.cpp
	src/threads/audio.cpp
	src/threads/input.cpp
	src/main.cpp
)
if(UNIX)
	target_sources(Playnote PRIVATE src/lib/pipewire.cpp)
endif()
target_precompile_headers(Playnote PRIVATE src/preamble.hpp)
target_include_directories(Playnote PRIVATE src) # All includes start from src as root

target_link_libraries(Playnote PRIVATE signalsmith-basics)
target_link_libraries(Playnote PRIVATE vk-bootstrap)
target_link_libraries(Playnote PRIVATE msd_channel)
target_link_libraries(Playnote PRIVATE PkgConfig::libswresample)
target_link_libraries(Playnote PRIVATE PkgConfig::libavformat)
target_link_libraries(Playnote PRIVATE PkgConfig::libavcodec)
target_link_libraries(Playnote PRIVATE PkgConfig::libavutil)
if(UNIX)
	target_link_libraries(Playnote PRIVATE PkgConfig::PipeWire)
endif()
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
if(WIN32)
	target_link_libraries(Playnote PRIVATE ksuser)
	target_link_libraries(Playnote PRIVATE winmm)
	target_link_libraries(Playnote PRIVATE avrt)
endif()
target_compile_definitions(imgui PUBLIC "IMGUI_USER_CONFIG=\"${PROJECT_SOURCE_DIR}/src/lib/imconfig.h\"")

# Assets and other artifacts

include(cmake/Shaders.cmake)
add_dependencies(Playnote Playnote_Shaders)
target_include_directories(Playnote PUBLIC ${PROJECT_BINARY_DIR}/$<CONFIG>/generated)
include(cmake/Assets.cmake)
add_dependencies(Playnote Playnote_Assets)

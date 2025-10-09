# This software is dual-licensed. For more details, please consult LICENSE.txt.
# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# CMakeLists.txt:
# Declares project files and build configuration.

cmake_minimum_required(VERSION 3.28)
project(Playnote
	VERSION 0.0.3
	DESCRIPTION "A BMS player hobby project"
	LANGUAGES C CXX)
include(cmake/Flags.cmake)
include(cmake/Dependencies.cmake)

add_executable(Playnote WIN32
	src/lib/archive.cpp
	src/lib/ebur128.cpp
	src/lib/openssl.cpp
	src/lib/sqlite.cpp
	src/lib/ffmpeg.cpp
	src/lib/vulkan.cpp
	src/lib/debug.cpp
	src/lib/imgui.cpp
	src/lib/glfw.cpp
	src/lib/icu.cpp
	src/lib/vuk.cpp
	src/lib/os.cpp
	src/threads/render.cpp
	src/threads/audio.cpp
	src/threads/input.cpp
	src/config.cpp
	src/main.cpp
)
if(UNIX)
	target_sources(Playnote PRIVATE
		src/lib/pipewire.cpp)
else()
	target_sources(Playnote PRIVATE
		src/lib/wasapi.cpp)
endif()

target_precompile_headers(Playnote PRIVATE src/preamble.hpp)
target_include_directories(Playnote PRIVATE src) # All includes start from src as root
# Communicate build type to the project
target_compile_definitions(Playnote PRIVATE "$<$<CONFIG:Debug>:BUILD_DEBUG>$<$<CONFIG:RelWithDebInfo>:BUILD_RELDEB>$<$<CONFIG:Release>:BUILD_RELEASE>")

target_link_libraries(Playnote PRIVATE signalsmith-basics)
target_link_libraries(Playnote PRIVATE tomlplusplus::tomlplusplus)
target_link_libraries(Playnote PRIVATE vk-bootstrap)
target_link_libraries(Playnote PRIVATE msd_channel)
target_link_libraries(Playnote PRIVATE LibArchive::LibArchive)
target_link_libraries(Playnote PRIVATE magic_enum::magic_enum)
target_link_libraries(Playnote PRIVATE libassert::assert)
target_link_libraries(Playnote PRIVATE zpp_bits)
target_link_libraries(Playnote PRIVATE OpenSSL::Crypto)
target_link_libraries(Playnote PRIVATE ebur128)
target_link_libraries(Playnote PRIVATE SQLite::SQLite3)
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
if(UNIX)
	target_link_libraries(Playnote PRIVATE PkgConfig::libswresample)
	target_link_libraries(Playnote PRIVATE PkgConfig::libavformat)
	target_link_libraries(Playnote PRIVATE PkgConfig::libavcodec)
	target_link_libraries(Playnote PRIVATE PkgConfig::libavutil)
	target_link_libraries(Playnote PRIVATE PkgConfig::PipeWire)
else()
	target_include_directories(Playnote PRIVATE ${FFMPEG_INCLUDE_DIRS})
	target_link_directories(Playnote PRIVATE ${FFMPEG_LIBRARY_DIRS})
	target_link_libraries(Playnote PRIVATE ${FFMPEG_LIBRARIES})
	target_link_libraries(Playnote PRIVATE ksuser)
	target_link_libraries(Playnote PRIVATE winmm)
	target_link_libraries(Playnote PRIVATE avrt)
endif()
target_compile_definitions(imgui PUBLIC "IMGUI_USER_CONFIG=\"${PROJECT_SOURCE_DIR}/src/lib/imconfig.h\"")

include(cmake/Shaders.cmake)
add_dependencies(Playnote Playnote_Shaders)
target_include_directories(Playnote PRIVATE ${PROJECT_BINARY_DIR}/$<CONFIG>/generated)
include(cmake/Assets.cmake)
add_dependencies(Playnote Playnote_Assets)

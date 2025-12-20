# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
# or https://www.mozilla.org/en-US/MPL/2.0/>. This file may not be copied, modified, or distributed
# except according to those terms.

cmake_minimum_required(VERSION 3.28)
project(Playnote
	VERSION 0.0.4
	DESCRIPTION "A BMS player hobby project"
	LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(cmake/Flags.cmake)
include(cmake/Dependencies.cmake)

add_executable(Playnote WIN32
	src/lib/signalsmith.cpp
	src/lib/harfbuzz.cpp
	src/lib/archive.cpp
	src/lib/ebur128.cpp
	src/lib/openssl.cpp
	src/lib/sqlite.cpp
	src/lib/ffmpeg.cpp
	src/lib/vulkan.cpp
	src/lib/debug.cpp
	src/lib/imgui.cpp
	src/lib/msdf.cpp
	src/lib/glfw.cpp
	src/lib/zstd.cpp
	src/lib/icu.cpp
	src/lib/vuk.cpp
	src/lib/os.cpp
	src/dev/controller.cpp
	src/dev/window.cpp
	src/dev/audio.cpp
	src/dev/gpu.cpp
	src/io/source.cpp
	src/io/file.cpp
	src/io/song.cpp
	src/audio/renderer.cpp
	src/audio/player.cpp
	src/audio/mixer.cpp
	src/gpu/shaders.cpp
	src/gfx/playfield.cpp
	src/gfx/renderer.cpp
	src/gfx/text.cpp
	src/bms/builder.cpp
	src/bms/library.cpp
	src/bms/cursor.cpp
	src/bms/mapper.cpp
	src/bms/score.cpp
	src/utils/config.cpp
	src/utils/logger.cpp
	src/render.cpp
	src/input.cpp
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

target_link_libraries(Playnote
	PRIVATE $<IF:$<TARGET_EXISTS:mimalloc-static>,mimalloc-static,mimalloc>
	PRIVATE signalsmith-basics
	PRIVATE readerwriterqueue::readerwriterqueue
	PRIVATE concurrentqueue::concurrentqueue
	PRIVATE msdf-atlas-gen::msdf-atlas-gen
	PRIVATE tomlplusplus::tomlplusplus
	PRIVATE vk-bootstrap
	PRIVATE LibArchive::LibArchive
	PRIVATE magic_enum::magic_enum
	PRIVATE libassert::assert
	PRIVATE Freetype::Freetype
	PRIVATE harfbuzz::harfbuzz
	PRIVATE OpenSSL::Crypto
	PRIVATE PkgConfig::ebur128
	PRIVATE libcoro
	PRIVATE unofficial::sqlite3::sqlite3
	PRIVATE Vulkan::Headers
	PRIVATE implot
	PRIVATE Boost::container
	PRIVATE Boost::boost
	PRIVATE quill::quill
	PRIVATE imgui::imgui
	PRIVATE zstd::libzstd
	PRIVATE volk::volk_headers
	PRIVATE volk::volk
	PRIVATE glfw
	PRIVATE ICU::i18n
	PRIVATE ICU::uc
	PRIVATE mio::mio-headers
	PRIVATE mio::mio
	PRIVATE vuk
	${FFMPEG_LIBRARIES}
)
if(UNIX)
	target_link_libraries(Playnote
		PRIVATE PkgConfig::PipeWire
	)
else()
	target_link_libraries(Playnote PRIVATE
		PRIVATE ksuser
		PRIVATE winmm
		PRIVATE avrt
	)
endif()
target_include_directories(Playnote
	PRIVATE ${FFMPEG_INCLUDE_DIRS}
	PRIVATE ${ZPP_BITS_INCLUDE_DIRS}
	PRIVATE ${PLF_COLONY_INCLUDE_DIRS})
target_link_directories(Playnote PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_compile_definitions(imgui::imgui INTERFACE "IMGUI_USER_CONFIG=\"${PROJECT_SOURCE_DIR}/src/lib/imconfig.h\"")

include(cmake/PlaynoteAssets.cmake)
add_dependencies(Playnote PlaynoteAssets)
target_include_directories(Playnote PRIVATE ${PROJECT_BINARY_DIR}/$<CONFIG>/generated)

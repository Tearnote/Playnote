# Copyright (c) 2026 Tearnote (Hubert Maraszek)
#
# Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
# or https://www.mozilla.org/en-US/MPL/2.0/>. This file may not be copied, modified, or distributed
# except according to those terms.

# Define project
cmake_minimum_required(VERSION 3.28)
set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON)
project(Playnote
	VERSION 0.0.4
	DESCRIPTION "A BMS player hobby project"
	LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(cmake/Flags.cmake)
include(cmake/Dependencies.cmake)

# Define target
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
	src/utils/assets.cpp
	src/render.cpp
	src/input.cpp
	src/main.cpp
)
if(NOT WIN32)
	target_sources(Playnote PRIVATE
		src/lib/pipewire.cpp)
else()
	target_sources(Playnote PRIVATE
		src/lib/wasapi.cpp)
endif()

# Configure target
target_precompile_headers(Playnote PRIVATE src/preamble.hpp)
target_include_directories(Playnote PRIVATE src) # All includes start from src as root
# Communicate build type to the project
target_compile_definitions(Playnote PRIVATE "$<$<CONFIG:Debug>:BUILD_DEBUG>$<$<CONFIG:RelWithDebInfo>:BUILD_RELDEB>$<$<CONFIG:Release>:BUILD_RELEASE>")

# Add dependencies
target_link_libraries(Playnote
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
if(NOT WIN32)
	target_link_libraries(Playnote
		PRIVATE Fontconfig::Fontconfig
		PRIVATE PkgConfig::PipeWire
		PRIVATE mimalloc-static
	)
else()
	target_link_libraries(Playnote PRIVATE
		PRIVATE mimalloc
		PRIVATE dwrite
		PRIVATE ksuser
		PRIVATE winmm
		PRIVATE avrt
	)
endif()
target_include_directories(Playnote
	PRIVATE ${FFMPEG_INCLUDE_DIRS}
	PRIVATE ${ZPP_BITS_INCLUDE_DIRS}
	PRIVATE ${PLF_COLONY_INCLUDE_DIRS}
)
target_link_directories(Playnote PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_compile_definitions(imgui::imgui INTERFACE "IMGUI_USER_CONFIG=\"${PROJECT_SOURCE_DIR}/src/lib/imconfig.h\"")

# Prepare assets
include(cmake/PlaynoteAssets.cmake)
add_dependencies(Playnote PlaynoteAssets)
target_include_directories(Playnote PRIVATE ${PROJECT_BINARY_DIR}/$<CONFIG>/generated)
include(cmake/PackAssets.cmake)
add_dependencies(Playnote PackAssets)

# Package the finished build
set(CMAKE_INSTALL_PREFIX "${PROJECT_BINARY_DIR}/install")
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP ON)
set(CMAKE_INSTALL_DEBUG_LIBRARIES ON)
install(TARGETS Playnote RUNTIME
	DESTINATION $<CONFIG>)
install(FILES "${PROJECT_BINARY_DIR}/$<CONFIG>/assets.db"
	DESTINATION $<CONFIG>)
if(WIN32)
	include(InstallRequiredSystemLibraries)
	set(MSVC_DEBUG_LIBS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
	list(FILTER MSVC_DEBUG_LIBS INCLUDE REGEX ".*[dD](_.*)?\\.dll$")
	set(MSVC_RELEASE_LIBS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
	list(FILTER MSVC_RELEASE_LIBS EXCLUDE REGEX ".*[dD](_.*)?\\.dll$")

	install(FILES ${MSVC_DEBUG_LIBS}
		DESTINATION $<CONFIG>
		CONFIGURATIONS Debug)
	install(FILES ${MSVC_RELEASE_LIBS}
		DESTINATION $<CONFIG>
		CONFIGURATIONS Release RelWithDebInfo)
endif()

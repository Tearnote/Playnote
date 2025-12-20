# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
# or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
# or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
# or distributed except according to those terms.

include_guard()

# Package systems

include(FetchContent)
find_package(PkgConfig REQUIRED)

# Local dependencies

find_package(Python3 COMPONENTS Interpreter REQUIRED) # For build-time scripts
if(UNIX)
	pkg_search_module(PipeWire REQUIRED IMPORTED_TARGET libpipewire-0.3) # Low latency Linux audio
endif()

# vcpkg dependencies

find_package(LibArchive REQUIRED) # Archive read/write support
find_package(OpenSSL REQUIRED) # MD5 hash
find_package(unofficial-sqlite3 CONFIG REQUIRED) # Local database
find_package(zstd CONFIG REQUIRED) # Lossless compression
find_package(VulkanHeaders CONFIG REQUIRED)
find_package(ICU REQUIRED # Charset detection and conversion, grapheme cluster iteration
	COMPONENTS uc i18n)
find_package(glfw3 CONFIG REQUIRED) # Windowing support
find_package(Boost CONFIG REQUIRED # Rational numbers, improved containers, string algorithms, resource wrapper
	COMPONENTS container)
find_package(Freetype REQUIRED) # Font file processing
find_package(harfbuzz CONFIG REQUIRED) # Text shaping
find_package(FFMPEG REQUIRED) # Sample rate conversion, audio file decoding
find_package(magic_enum CONFIG REQUIRED) # Enum reflection
find_package(libassert CONFIG REQUIRED) # Smarter assert macros
find_package(mio CONFIG REQUIRED) # Memory-mapped disk IO
find_package(quill CONFIG REQUIRED) # Threaded logger
find_package(volk CONFIG REQUIRED) # Vulkan loader
pkg_search_module(ebur128 REQUIRED IMPORTED_TARGET libebur128) # Volume normalization
find_package(vk-bootstrap CONFIG REQUIRED) # Vulkan initialization
find_package(imgui CONFIG REQUIRED) # Debug controls
target_link_libraries(imgui::imgui INTERFACE libassert::assert)
find_package(implot CONFIG REQUIRED) # Debug plot drawing
find_package(tomlplusplus CONFIG REQUIRED) # TOML config file parsing
find_path(ZPP_BITS_INCLUDE_DIRS "zpp_bits.h") # Serialization
find_package(readerwriterqueue CONFIG REQUIRED) # Lock-free SPSC queue
find_package(concurrentqueue CONFIG REQUIRED) # Lock-free MPMC queue
find_path(PLF_COLONY_INCLUDE_DIRS "plf_colony.h") # Memory-stable unordered container
find_package(mimalloc CONFIG REQUIRED) # High performance allocator

# Fix ebur128 on Windows
if(WIN32)
	get_target_property(EBUR128_LIBS PkgConfig::ebur128 INTERFACE_LINK_LIBRARIES)
	list(REMOVE_ITEM EBUR128_LIBS "m")
	set_target_properties(PkgConfig::ebur128 PROPERTIES INTERFACE_LINK_LIBRARIES "${EBUR128_LIBS}")
endif()

# Remote dependencies

set(VUK_LINK_TO_LOADER OFF CACHE BOOL "" FORCE)
set(VUK_USE_VULKAN_SDK OFF CACHE BOOL "" FORCE)
set(VUK_USE_SHADERC OFF CACHE BOOL "" FORCE)
set(VUK_EXTRA_IMGUI OFF CACHE BOOL "" FORCE)
set(VUK_EXTRA_INIT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(vuk # Vulkan rendergraph
	GIT_REPOSITORY https://github.com/martty/vuk
	GIT_TAG f3af1ba601532f93f07dcef49f25d9469e4cd1f6
)
FetchContent_MakeAvailable(vuk)
if(UNIX)
	target_compile_options(vuk PRIVATE -g0) # Work around bug in Embed module
endif()
target_compile_definitions(vuk PUBLIC VUK_CUSTOM_VULKAN_HEADER=<volk.h>)
target_link_libraries(vuk
	PRIVATE Vulkan::Headers
	PRIVATE volk::volk_headers
	PRIVATE volk::volk)

FetchContent_Declare(signalsmith-basics # Audio processing
	GIT_REPOSITORY https://github.com/Signalsmith-Audio/basics
	GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(signalsmith-basics)

FetchContent_Declare(libcoro # Coroutine primitives
	GIT_REPOSITORY https://github.com/jbaldwin/libcoro
	GIT_TAG 7e0ce982405fb26b6ca8af97f40a8eaa2b78c4fa
)
set(LIBCORO_FEATURE_NETWORKING OFF CACHE BOOL "")
FetchContent_MakeAvailable(libcoro)

set(MSDF_ATLAS_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
set(MSDF_ATLAS_NO_ARTERY_FONT ON CACHE BOOL "" FORCE)
set(MSDF_ATLAS_USE_SKIA OFF CACHE BOOL "" FORCE)
FetchContent_Declare(msdf-atlas-gen # Font atlas generation
	GIT_REPOSITORY https://github.com/Chlumsky/msdf-atlas-gen
	GIT_TAG v1.3
)
FetchContent_MakeAvailable(msdf-atlas-gen)

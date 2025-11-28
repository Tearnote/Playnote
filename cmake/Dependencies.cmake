# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
# or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
# or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
# or distributed except according to those terms.

include_guard()

if(UNIX)
	find_package(PkgConfig REQUIRED)
endif()
include(FetchContent)

# Local dependencies

find_package(LibArchive REQUIRED) # Archive read/write support
find_package(OpenSSL REQUIRED) # MD5 hash
find_package(SQLite3 REQUIRED) # Local database
find_package(zstd REQUIRED) # Lossless compression
if(WIN32)
	find_package(VulkanHeaders REQUIRED)
else()
	find_package(Vulkan REQUIRED) # GPU API
endif()
find_package(ICU REQUIRED # Charset detection and conversion
	COMPONENTS uc i18n
)
find_package(glfw3 REQUIRED) # Windowing support
find_package(Boost REQUIRED # Rational numbers, improved containers, string algorithms, resource wrapper
	COMPONENTS container)
if(UNIX)
	pkg_search_module(libswresample REQUIRED IMPORTED_TARGET libswresample) # Sample rate conversion
	pkg_search_module(libavformat REQUIRED IMPORTED_TARGET libavformat) # Audio file demuxing
	pkg_search_module(libavcodec REQUIRED IMPORTED_TARGET libavcodec) # Audio file decoding
	pkg_search_module(libavutil REQUIRED IMPORTED_TARGET libavutil) # FFmpeg utilities
else()
	find_package(FFMPEG REQUIRED)
endif()

if(UNIX)
	pkg_search_module(PipeWire REQUIRED IMPORTED_TARGET libpipewire-0.3) # Low latency Linux audio
endif()

# Remote dependencies

FetchContent_Declare(libassert # Smarter assert macros
	GIT_REPOSITORY https://github.com/jeremy-rifkin/libassert
	GIT_TAG v2.1.5
)
FetchContent_MakeAvailable(libassert)

FetchContent_Declare(mio # Memory-mapped disk IO
	GIT_REPOSITORY https://github.com/vimpunk/mio
	GIT_TAG 8b6b7d878c89e81614d05edca7936de41ccdd2da
)
FetchContent_MakeAvailable(mio)

FetchContent_Declare(quill # Threaded logger
	GIT_REPOSITORY https://github.com/odygrd/quill
	GIT_TAG v10.1.0
)
FetchContent_MakeAvailable(quill)

set(VOLK_PULL_IN_VULKAN OFF CACHE BOOL "" FORCE)
FetchContent_Declare(volk # Vulkan loader
	GIT_REPOSITORY https://github.com/zeux/volk
	GIT_TAG 1.4.304
)
FetchContent_MakeAvailable(volk)
target_compile_definitions(volk PUBLIC VK_NO_PROTOTYPES)
target_link_libraries(volk PUBLIC Vulkan::Headers)

FetchContent_Declare(vk-bootstrap # Vulkan initialization
	GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
	GIT_TAG v1.4.311
)
FetchContent_MakeAvailable(vk-bootstrap)

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
target_link_libraries(vuk PRIVATE volk)

FetchContent_Declare(ebur128 # Volume normalization
	GIT_REPOSITORY https://github.com/jiixyj/libebur128
	GIT_TAG v1.2.6
	SOURCE_SUBDIR "NONEXISTENT" # Bad CMakeLists.txt, creating target manually
)
FetchContent_MakeAvailable(ebur128)
add_library(ebur128 ${ebur128_SOURCE_DIR}/ebur128/ebur128.c)
target_include_directories(ebur128 PUBLIC ${ebur128_SOURCE_DIR}/ebur128)
# Windows portability
include(CheckIncludeFile)
check_include_file(sys/queue.h HAVE_SYS_QUEUE_H)
if(NOT HAVE_SYS_QUEUE_H)
	target_include_directories(ebur128 PRIVATE ${ebur128_SOURCE_DIR}/ebur128/queue)
endif()
if(WIN32)
	target_compile_definitions(ebur128 PRIVATE _USE_MATH_DEFINES)
endif()

FetchContent_Declare(imgui # Debug controls
	GIT_REPOSITORY https://github.com/ocornut/imgui
	GIT_TAG v1.91.9b
)
FetchContent_MakeAvailable(imgui)
add_library(imgui
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_demo.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
)
target_include_directories(imgui PUBLIC "${imgui_SOURCE_DIR}")
target_link_libraries(imgui PUBLIC libassert::assert)
target_link_libraries(imgui PUBLIC glfw)

FetchContent_Declare(implot # Debug plot drawing
	GIT_REPOSITORY https://github.com/epezent/implot
	GIT_TAG 47522f47054d33178e7defa780042bd2a06b09f9
)
FetchContent_MakeAvailable(implot)
add_library(implot
	${implot_SOURCE_DIR}/implot.cpp
	${implot_SOURCE_DIR}/implot_items.cpp
)
target_include_directories(implot PUBLIC "${implot_SOURCE_DIR}")
target_link_libraries(implot PRIVATE imgui)

FetchContent_Declare(signalsmith-basics # Audio processing
	GIT_REPOSITORY https://github.com/Signalsmith-Audio/basics
	GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(signalsmith-basics)

FetchContent_Declare(tomlplusplus # TOML config file parsing
	GIT_REPOSITORY https://github.com/marzer/tomlplusplus
	GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

FetchContent_Declare(magic_enum # Enum reflection
	GIT_REPOSITORY https://github.com/Neargye/magic_enum
	GIT_TAG v0.9.7
)
FetchContent_MakeAvailable(magic_enum)

FetchContent_Declare(zpp_bits # Serialization
	GIT_REPOSITORY https://github.com/eyalz800/zpp_bits
	GIT_TAG v4.5.1
)
FetchContent_MakeAvailable(zpp_bits)
add_library(zpp_bits INTERFACE ${zpp_bits_SOURCE_DIR}/zpp_bits.h)
target_include_directories(zpp_bits INTERFACE ${zpp_bits_SOURCE_DIR})

FetchContent_Declare(libcoro # Coroutine primitives
	GIT_REPOSITORY https://github.com/jbaldwin/libcoro
	GIT_TAG 7e0ce982405fb26b6ca8af97f40a8eaa2b78c4fa
)
set(LIBCORO_FEATURE_NETWORKING OFF CACHE BOOL "")
FetchContent_MakeAvailable(libcoro)

FetchContent_Declare(readerwriterqueue # Lock-free SPSC queue
	GIT_REPOSITORY https://github.com/cameron314/readerwriterqueue
	GIT_TAG v1.0.7
)
FetchContent_MakeAvailable(readerwriterqueue)

FetchContent_Declare(concurrentqueue # Lock-free MPMC queue
	GIT_REPOSITORY https://github.com/cameron314/concurrentqueue
	GIT_TAG v1.0.4
)
FetchContent_MakeAvailable(concurrentqueue)

FetchContent_Declare(colony # Memory-stable unordered container
	GIT_REPOSITORY https://github.com/mattreecebentley/plf_colony
	GIT_TAG 1a5b13268e0e2f97605ef130b2159aa4ed5c6eba
)
FetchContent_MakeAvailable(colony)
add_library(colony INTERFACE ${colony_SOURCE_DIR}/plf_colony.h)
target_include_directories(colony INTERFACE ${colony_SOURCE_DIR})

set(MSDF_ATLAS_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
set(MSDF_ATLAS_USE_VCPKG OFF CACHE BOOL "" FORCE)
set(MSDF_ATLAS_USE_SKIA OFF CACHE BOOL "" FORCE)
set(MSDF_ATLAS_NO_ARTERY_FONT ON CACHE BOOL "" FORCE)
find_package(Freetype REQUIRED)
find_package(PNG REQUIRED)
FetchContent_Declare(msdf-atlas-gen # Font atlas generation
	GIT_REPOSITORY https://github.com/Chlumsky/msdf-atlas-gen
	GIT_TAG v1.3
)
FetchContent_MakeAvailable(msdf-atlas-gen)

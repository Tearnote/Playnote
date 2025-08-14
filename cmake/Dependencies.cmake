# This software is dual-licensed. For more details, please consult LICENSE.txt.
# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# cmake/Dependencies.cmake:
# Declares and retrieves all external dependencies.

include_guard()

find_package(PkgConfig REQUIRED)
include(FetchContent)

# Local dependencies

find_package(OpenSSL REQUIRED) # MD5 hash
find_package(Vulkan REQUIRED # GPU API
	COMPONENTS glslc glslangValidator # Shader compiler
)
find_package(ICU REQUIRED # Charset detection and conversion
	COMPONENTS uc i18n
)
find_package(glfw3 3.4 REQUIRED) # Windowing support
find_package(Boost REQUIRED # Rational numbers, improved containers, string algorithms, resource wrapper
	COMPONENTS container)
pkg_search_module(libswresample REQUIRED IMPORTED_TARGET libswresample) # Sample rate conversion
pkg_search_module(libavformat REQUIRED IMPORTED_TARGET libavformat) # Audio file demuxing
pkg_search_module(libavcodec REQUIRED IMPORTED_TARGET libavcodec) # Audio file decoding
pkg_search_module(libavutil REQUIRED IMPORTED_TARGET libavutil) # FFmpeg utilities

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
	GIT_TAG v9.0.3
)
FetchContent_MakeAvailable(quill)

set(VOLK_PULL_IN_VULKAN OFF CACHE BOOL "" FORCE)
FetchContent_Declare(volk # Vulkan loader
	GIT_REPOSITORY https://github.com/zeux/volk
	GIT_TAG 1.4.304
)
FetchContent_MakeAvailable(volk)
target_compile_definitions(volk PUBLIC VK_NO_PROTOTYPES)
target_include_directories(volk PUBLIC ${Vulkan_INCLUDE_DIRS})

FetchContent_Declare(vk-bootstrap # Vulkan initialization
	GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
	GIT_TAG v1.4.311
)
FetchContent_MakeAvailable(vk-bootstrap)

set(VUK_LINK_TO_LOADER OFF CACHE BOOL "" FORCE)
set(VUK_USE_SHADERC OFF CACHE BOOL "" FORCE)
set(VUK_EXTRA_IMGUI OFF CACHE BOOL "" FORCE)
set(VUK_EXTRA_INIT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(vuk # Vulkan rendergraph
	GIT_REPOSITORY https://github.com/martty/vuk
	GIT_TAG 4b88bb86ed3a6c40f21b3ee355f258c10f71e998
)
FetchContent_MakeAvailable(vuk)
target_compile_options(vuk PRIVATE -g0) # Work around bug in Embed module
target_compile_definitions(vuk PUBLIC VUK_CUSTOM_VULKAN_HEADER=<volk.h>)
target_link_libraries(vuk PRIVATE volk)

set(TRACY_ONLY_LOCALHOST ON CACHE BOOL "" FORCE)
set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)
FetchContent_Declare(tracy # CPU/GPU profiler
	GIT_REPOSITORY https://github.com/wolfpld/tracy
	GIT_TAG 53510c316bd48b7899f15c98a510ad632124fc58
)
FetchContent_MakeAvailable(tracy)
target_compile_definitions(TracyClient PUBLIC TRACY_VK_USE_SYMBOL_TABLE)

FetchContent_Declare(cpp-channels # Threaded data sharing
	GIT_REPOSITORY https://github.com/andreiavrammsd/cpp-channel
	GIT_TAG v0.8.3
)
FetchContent_MakeAvailable(cpp-channels)

FetchContent_Declare(ebur128 # Volume normalization
	GIT_REPOSITORY https://github.com/jiixyj/libebur128
	GIT_TAG v1.2.6
	SOURCE_SUBDIR "NONEXISTENT" # Bad CMakeLists.txt, creating target manually
)
FetchContent_MakeAvailable(ebur128)
add_library(ebur128 ${ebur128_SOURCE_DIR}/ebur128/ebur128.c)
target_include_directories(ebur128 PUBLIC ${ebur128_SOURCE_DIR}/ebur128)

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

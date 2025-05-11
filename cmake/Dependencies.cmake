# Declaration of all external dependencies
include_guard()

find_package(PkgConfig REQUIRED)
include(FetchContent)

# Local dependencies

find_package(Vulkan REQUIRED # GPU API
	COMPONENTS glslc glslangValidator # Shader compiler
)
find_package(glfw3 3.4 REQUIRED) # Windowing support
pkg_search_module(libsndfile REQUIRED IMPORTED_TARGET sndfile) # Audio file decoding

if(UNIX)
	pkg_search_module(PipeWire REQUIRED IMPORTED_TARGET libpipewire-0.3) # Low latency Linux audio
endif()

# Remote dependencies

FetchContent_Declare(libassert # Smarter assert macros
	GIT_REPOSITORY https://github.com/jeremy-rifkin/libassert
	GIT_TAG v2.1.5
)
FetchContent_MakeAvailable(libassert)

FetchContent_Declare(unordered_dense # std::unordered_map replacement
	GIT_REPOSITORY https://github.com/martinus/unordered_dense
	GIT_TAG v4.5.0
)
FetchContent_MakeAvailable(unordered_dense)

FetchContent_Declare(quill # Threaded logger
	GIT_REPOSITORY https://github.com/odygrd/quill
	GIT_TAG v8.2.0
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
set(VUK_FAIL_FAST ON CACHE BOOL "" FORCE)
set(VUK_EXTRA ON CACHE BOOL "" FORCE)
set(VUK_EXTRA_IMGUI ON CACHE BOOL "" FORCE)
set(VUK_EXTRA_IMGUI_PLATFORM_BACKEND "glfw" CACHE STRING "" FORCE)
set(VUK_EXTRA_INIT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(vuk # Vulkan rendergraph
	GIT_REPOSITORY https://github.com/martty/vuk
	GIT_TAG 4b88bb86ed3a6c40f21b3ee355f258c10f71e998
)
FetchContent_MakeAvailable(vuk)
target_compile_definitions(vuk PUBLIC VUK_CUSTOM_VULKAN_HEADER=<volk.h>)
target_link_libraries(vuk PRIVATE volk)

set(TRACY_ONLY_LOCALHOST ON CACHE BOOL "" FORCE)
FetchContent_Declare(tracy # CPU/GPU profiler
	GIT_REPOSITORY https://github.com/wolfpld/tracy
	GIT_TAG 53510c316bd48b7899f15c98a510ad632124fc58
	GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(tracy)
target_compile_definitions(TracyClient PUBLIC TRACY_VK_USE_SYMBOL_TABLE)

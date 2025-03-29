include_guard()

include(FetchContent)

### Local dependencies

find_package(Vulkan REQUIRED
	COMPONENTS glslc glslangValidator
)
find_package(glfw3 3.4 REQUIRED)

### Remote dependencies

set(VOLK_PULL_IN_VULKAN OFF CACHE BOOL "" FORCE)
FetchContent_Declare(volk
	GIT_REPOSITORY https://github.com/zeux/volk
	GIT_TAG 1.4.304
)
FetchContent_MakeAvailable(volk)
target_compile_definitions(volk PUBLIC VK_NO_PROTOTYPES)
target_include_directories(volk PUBLIC ${Vulkan_INCLUDE_DIRS})

FetchContent_Declare(vk-bootstrap
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
FetchContent_Declare(vuk
	GIT_REPOSITORY https://github.com/martty/vuk
	GIT_TAG 4ad9d7714b1b2489f11be9f264e84e2073f30492
)
FetchContent_MakeAvailable(vuk)
target_compile_definitions(vuk PUBLIC VUK_CUSTOM_VULKAN_HEADER=<volk.h>)
target_link_libraries(vuk PRIVATE volk)

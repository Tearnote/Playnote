### Project configuration

cmake_minimum_required(VERSION 3.30)
project(Playnote
	VERSION 0.0.1
	DESCRIPTION "A BMS player hobby project"
	LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

### Global build settings

set(BUILD_SHARED_LIBS OFF) # Only build static libs
#TODO Compiler and linker flags:
#TODO - nonstandard speedups (fastmath etc)
#TODO - enable exceptions, disable RTTI
#TODO - safety checks for DEBUG
#TODO - optimizations for RELEASE
#TODO - enable LTO and dead code removal
# Communicate build type to the project
add_compile_definitions("BUILD_TYPE=$<$<CONFIG:Debug>:BUILD_DEBUG>$<$<CONFIG:RelWithDebInfo>:BUILD_RELDEB>$<$<CONFIG:Release>:BUILD_RELEASE>")

include(cmake/Dependencies.cmake)

### Main target

add_executable(Playnote
	src/util/concepts.hpp
	src/util/service.hpp
	src/util/except.hpp
	src/util/logger.hpp
	src/util/logger.cpp
	src/util/ranges.hpp
	src/util/types.hpp
	src/util/math.hpp
	src/util/time.hpp
	src/sys/window.cpp
	src/sys/window.hpp
	src/sys/os.hpp
	src/sys/os.cpp
	src/config.hpp
	src/main.cpp
)

target_include_directories(Playnote PRIVATE src) # All includes start from src as root

target_link_libraries(Playnote PRIVATE vk-bootstrap)
target_link_libraries(Playnote PRIVATE libassert::assert)
target_link_libraries(Playnote PRIVATE quill)
target_link_libraries(Playnote PRIVATE glfw)
target_link_libraries(Playnote PRIVATE volk)
target_link_libraries(Playnote PRIVATE vuk)

### Assets and other artifacts

include(cmake/Shaders.cmake)
add_dependencies(Playnote Playnote_Shaders)
target_include_directories(Playnote PUBLIC ${PROJECT_BINARY_DIR}/$<CONFIG>/generated)
include(cmake/Assets.cmake)
add_dependencies(Playnote Playnote_Assets)

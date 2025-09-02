# This software is dual-licensed. For more details, please consult LICENSE.txt.
# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# cmake/Flags.cmake:
# Project-wide compiler flags.

include_guard()

# Language standards

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Global build settings

set(BUILD_SHARED_LIBS OFF)
if(UNIX)
	add_compile_options("-Wall;-Wextra;-Wno-missing-field-initializers")
	add_compile_options("-march=x86-64-v3;-ffast-math")
	add_compile_options("-fno-strict-aliasing;$<$<COMPILE_LANGUAGE:CXX>:-fexceptions;-frtti>")
	add_compile_options("$<$<NOT:$<CONFIG:Release>>:-g>")
	add_compile_options("$<$<CONFIG:Release>:-g0>;$<$<NOT:$<CONFIG:Debug>>:-O3>")
	add_link_options("$<$<NOT:$<CONFIG:Debug>>:-O3>")
else()
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	add_compile_options("-Wno-unqualified-std-cast-call;-Wno-nan-infinity-disabled")
	add_compile_options("/arch:AVX2;/fp:fast")
	add_compile_options("/EHsc;/GR")
	add_compile_options("$<IF:$<CONFIG:Debug>,/Od;/GS,/O2;/GS->")
	add_link_options("/MANIFEST:EMBED;/MANIFESTINPUT:${PROJECT_SOURCE_DIR}/Playnote.manifest")
endif()

# Copyright (c) 2026 Tearnote (Hubert Maraszek)
#
# Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
# or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
# or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
# or distributed except according to those terms.

include_guard()

include(FetchContent)

set(SLANG_VERSION 2025.24)

if(WIN32)
	set(SLANG_OS windows)
	set(SLANGC_FILENAME slangc.exe)
elseif(UNIX)
	set(SLANG_OS linux)
	set(SLANGC_FILENAME slangc)
else()
	message(FATAL_ERROR "Unexpected OS for Slang compiler download")
endif()

FetchContent_Declare(slangc
	URL https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-${SLANG_OS}-x86_64.zip
)
FetchContent_MakeAvailable(slangc)
set(SLANGC_EXECUTABLE "${slangc_SOURCE_DIR}/bin/${SLANGC_FILENAME}" CACHE FILEPATH "Path to slangc executable")

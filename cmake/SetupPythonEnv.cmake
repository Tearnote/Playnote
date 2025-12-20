# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
# or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
# or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
# or distributed except according to those terms.

include_guard()

include(cmake/Dependencies.cmake)

set(PYTHON_VENV_DIR "${PROJECT_BINARY_DIR}/python_venv")
set(PYTHON_REQUIREMENTS "${PROJECT_SOURCE_DIR}/tools/requirements.txt")
set(PYTHON_VENV_STAMP "${PROJECT_BINARY_DIR}/python_venv.stamp")

if(WIN32)
	set(PYTHON_VENV_EXE "${PYTHON_VENV_DIR}/Scripts/python.exe")
else()
	set(PYTHON_VENV_EXE "${PYTHON_VENV_DIR}/bin/python")
endif()

add_custom_command(
	OUTPUT ${PYTHON_VENV_STAMP}
	COMMAND ${Python3_EXECUTABLE} -m venv ${PYTHON_VENV_DIR}
	COMMAND ${PYTHON_VENV_EXE} -m pip install -r ${PYTHON_REQUIREMENTS}
	COMMAND ${CMAKE_COMMAND} -E touch ${PYTHON_VENV_STAMP}
	DEPENDS ${PYTHON_REQUIREMENTS}
	COMMENT "Setting up Python virtual environment"
)
add_custom_target(SetupPythonEnv DEPENDS ${PYTHON_VENV_STAMP})

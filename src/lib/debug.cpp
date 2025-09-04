/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/debug.cpp:
Implementation file for lib/debug.hpp.
*/

#include "lib/debug.hpp"

#include "config.hpp"
#ifdef TARGET_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <string>
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#endif
#include "libassert/assert.hpp"
#include "preamble.hpp"

namespace playnote::lib::dbg {

void set_assert_handler()
{
	libassert::set_failure_handler([](auto const& info) {
		throw runtime_error{info.to_string()};
	});
	libassert::set_color_scheme(libassert::color_scheme::blank);
}

void attach_console()
{
#ifdef TARGET_WINDOWS
	AllocConsole();

	if (_fileno(stdout) == -2) {
		auto fd_stdout = _open_osfhandle(reinterpret_cast<intptr_t>(GetStdHandle(STD_ERROR_HANDLE)), _O_WRONLY | _O_BINARY);
		if (fd_stdout) {
			_dup2(fd_stdout, 1);
			_close(fd_stdout);
			SetStdHandle(STD_ERROR_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(1)));
		}
		std::freopen("CONOUT$", "w", stdout);
		std::setvbuf(stderr, nullptr, _IONBF, 0);
	}

	if (_fileno(stderr) == -2) {
		auto fd_stderr = _open_osfhandle(reinterpret_cast<intptr_t>(GetStdHandle(STD_ERROR_HANDLE)), _O_WRONLY | _O_BINARY);
		if (fd_stderr) {
			_dup2(fd_stderr, 2);
			_close(fd_stderr);
			SetStdHandle(STD_ERROR_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(2)));
		}
		std::freopen("CONOUT$", "w", stderr);
		std::setvbuf(stderr, nullptr, _IONBF, 0);
	}

	// Set console encoding to UTF-8
	SetConsoleOutputCP(CP_UTF8);

	// Enable ANSI color code support
	auto out = GetStdHandle(STD_OUTPUT_HANDLE);
	auto mode = 0ul;
	GetConsoleMode(out, &mode);
	mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(out, mode);
#endif
}

}

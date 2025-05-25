/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/debug.cppm:
Wrapper for OS-specific debugging enablement.
*/

module;
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <string>
#include <cstdio>
#include <Windows.h>
#include <fcntl.h>
#include <io.h>
#endif

export module playnote.lib.debug;

namespace playnote::lib {

// Open the console window and attach standard outputs to it. Errors are ignored.
// https://github.com/ocaml/ocaml/issues/9252#issuecomment-576383814
export void attach_console() noexcept
{
#ifdef _WIN32
	AllocConsole();

	std::freopen("CONOUT$", "w", stdout);
	std::freopen("CONOUT$", "w", stderr);

	auto fdOut = _open_osfhandle(reinterpret_cast<intptr_t>(GetStdHandle(STD_OUTPUT_HANDLE)), _O_WRONLY | _O_BINARY);
	auto fdErr = _open_osfhandle(reinterpret_cast<intptr_t>(GetStdHandle(STD_ERROR_HANDLE)), _O_WRONLY | _O_BINARY);

	if (fdOut) {
		_dup2(fdOut, 1);
		_close(fdOut);
		SetStdHandle(STD_OUTPUT_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(1)));
	}
	if (fdErr) {
		_dup2(fdErr, 2);
		_close(fdErr);
		SetStdHandle(STD_ERROR_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(2)));
	}

	_dup2(_fileno(fdopen(1, "wb")), _fileno(stdout));
	_dup2(_fileno(fdopen(2, "wb")), _fileno(stderr));

	std::setvbuf(stdout, nullptr, _IONBF, 0);
	std::setvbuf(stderr, nullptr, _IONBF, 0);

	// Set console encoding to UTF-8
	SetConsoleOutputCP(65001);

	// Enable ANSI color code support
	auto out = GetStdHandle(STD_OUTPUT_HANDLE);
	auto mode = 0ul;
	GetConsoleMode(out, &mode);
	mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(out, mode);
#endif
}

}

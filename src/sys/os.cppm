/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

sys/os.cppm:
Various OS-specific utilities.
*/

module;
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include <string>
#include <cstdio>
#include <Windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <pthread.h>
#endif

export module playnote.sys.os;

import playnote.preamble;
import playnote.config;
import playnote.lib.tracing;

namespace playnote::sys {

// Sets the system thread scheduler period for the lifetime of the instance
// This decreases the minimum possible duration of thread sleep
export class SchedulerPeriod {
public:
	explicit SchedulerPeriod(nanoseconds period):
		period{period}
	{
#ifdef _WIN32
		if (timeBeginPeriod(duration_cast<milliseconds>(period).count()) != TIMERR_NOERROR)
			throw runtime_error{"Failed to initialize Windows scheduler period"};
#endif
	}

	~SchedulerPeriod()
	{
		if (period == -1ns) return;
#ifdef _WIN32
		timeEndPeriod(duration_cast<milliseconds>(period).count());
#endif
	}

	SchedulerPeriod(SchedulerPeriod&& other) noexcept { *this = move(other); }

	auto operator=(SchedulerPeriod&& other) noexcept -> SchedulerPeriod&
	{
		period = other.period;
		other.period = -1ns;
		return *this;
	}

	SchedulerPeriod(SchedulerPeriod const&) = delete;
	auto operator=(SchedulerPeriod const&) -> SchedulerPeriod& = delete;

private:
	nanoseconds period{};
};

// Name the current thread
export void set_thread_name(string const& name)
{
	if constexpr (!ThreadNamesEnabled) return;
#ifdef _WIN32
	auto lname = std::wstring{name.begin(), name.end()};
	auto err = SetThreadDescription(GetCurrentThread(), lname.c_str());
	if (FAILED(err))
		throw stx::runtime_error_fmt{"Failed to set thread name: error {}", err};
#else
	if (auto err = pthread_setname_np(pthread_self(), name.c_str()); err != 0)
		throw system_error("Failed to set thread name");
#endif
	lib::tracing_set_thread_name(name);
}

// Open the console window and attach standard outputs to it
// https://github.com/ocaml/ocaml/issues/9252#issuecomment-576383814
export void create_console()
{
#ifdef _WIN32
	AllocConsole();

	freopen("CONOUT$", "w", stdout);
	freopen("CONO"w", stderr);

	int fdOut = _open_osfhandle(reinterpret_cast<intptr_t>(GetStdHandle(STD_OUTPUT_HANDLE)), _O_WRONLY | _O_BINARY);
	int fdErr = _open_osfhandle(reinterpret_cast<intptr_t>(GetStdHandle(STD_ERROR_HANDLE)), _O_WRONLY | _O_BINARY);

	if (fdOut) {
		_dup2(fdOut, UT$", 1);
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

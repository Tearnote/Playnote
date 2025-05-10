module;
#include <string_view>
#include <chrono>
#include "config.hpp"

#if TARGET == TARGET_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include <cstdio>
#include <string>
#include <Windows.h>
#include <fcntl.h>
#include <io.h>
#elif TARGET == TARGET_LINUX
#include <system_error>
#include <string>
#include <pthread.h>
#endif

export module playnote.sys.os;

#if TARGET == TARGET_WINDOWS
import playnote.stx.except;
#endif

namespace playnote::sys {

namespace chrono = std::chrono;
using namespace std::chrono_literals;
using chrono::duration_cast;

// Sets the system thread scheduler period for the lifetime of the instance
// This decreases the minimum possible duration of thread sleep
export class SchedulerPeriod {
public:
	explicit SchedulerPeriod(chrono::nanoseconds period):
		period{period}
	{
#if TARGET == TARGET_WINDOWS
		if (timeBeginPeriod(duration_cast<chrono::milliseconds>(period).count()) != TIMERR_NOERROR)
			throw std::runtime_error{"Failed to initialize Windows scheduler period"};
#endif
	}

	~SchedulerPeriod()
	{
		if (period == -1ns) return;
#if TARGET == TARGET_WINDOWS
		timeEndPeriod(duration_cast<chrono::milliseconds>(period).count());
#endif
	}

	SchedulerPeriod(SchedulerPeriod&& other) noexcept { *this = std::move(other); }

	auto operator=(SchedulerPeriod&& other) noexcept -> SchedulerPeriod&
	{
		period = other.period;
		other.period = -1ns;
		return *this;
	}

	SchedulerPeriod(SchedulerPeriod const&) = delete;
	auto operator=(SchedulerPeriod const&) -> SchedulerPeriod& = delete;

private:
	chrono::nanoseconds period{};
};

// Name the current thread
export void set_thread_name(std::string_view name)
{
#ifdef THREAD_DEBUG
#if TARGET == TARGET_WINDOWS
	auto lname = std::wstring{name.begin(), name.end()};
	auto err = SetThreadDescription(GetCurrentThread(), lname.c_str());
	if (FAILED(err))
		throw stx::runtime_error_fmt{"Failed to set thread name: error {}", err};
#elif TARGET == TARGET_LINUX
	if (auto err = pthread_setname_np(pthread_self(), std::string{name}.c_str()); err != 0)
		throw std::system_error{err, std::system_category(), "Failed to set thread name"};
#endif
#endif
}

// Open the console window and attach standard outputs to it
// https://github.com/ocaml/ocaml/issues/9252#issuecomment-576383814
export void create_console()
{
#if TARGET == TARGET_WINDOWS
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

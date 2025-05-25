/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/thread.cppm:
Wrapper for OS-specific thread handling functions not provided by STL.
*/

module;
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <pthread.h>
#endif

export module playnote.lib.thread;

import playnote.preamble;

namespace playnote::lib {

// Set thread name in the OS scheduler, which can help with debugging.
// Throws runtime_error on failure.
export void set_thread_name(string_view name)
{
#ifdef _WIN32
	auto const lname = std::wstring{name.begin(), name.end()};
	auto const err = SetThreadDescription(GetCurrentThread(), lname.c_str());
	if (FAILED(err))
		throw runtime_error_fmt{"Failed to set thread name: error {}", err};
#else
	auto const err = pthread_setname_np(pthread_self(), string{name}.c_str());
	if (err != 0)
		throw system_error("Failed to set thread name");
#endif
}

// Set the thread scheduler period to at most the provided value. This can increase the resolution
// of thread sleep and yield. Pair with a matching callto end_thread_scheduler_period
// with the same period.
// Throws runtime_error on failure.
export void begin_thread_scheduler_period([[maybe_unused]] milliseconds period)
{
#ifdef _WIN32
	if (timeBeginPeriod(period.count()) != TIMERR_NOERROR)
		throw runtime_error{"Failed to initialize thread scheduler period"};
#endif
}

// End a previously started thread scheduler period. Failure is ignored.
export void end_thread_scheduler_period([[maybe_unused]] milliseconds period) noexcept
{
#ifdef _WIN32
	timeEndPeriod(period.count());
#endif
}

}

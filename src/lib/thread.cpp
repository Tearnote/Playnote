/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/thread.cpp:
Implementation file for lib/thread.hpp.
*/

#include "lib/thread.hpp"

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
#include "preamble.hpp"
#include "config.hpp"

namespace playnote::lib::thread {

void name_current(string_view name)
{
#ifdef _WIN32
	auto const lname = std::wstring{name.begin(), name.end()};
	auto const err = SetThreadDescription(GetCurrentThread(), lname.c_str());
	if (FAILED(err))
		throw runtime_error_fmt("Failed to set thread name: error {}", err);
#else
	auto const err = pthread_setname_np(pthread_self(), string{name}.c_str());
	if (err != 0)
		throw system_error("Failed to set thread name");
#endif
}

void begin_scheduler_period([[maybe_unused]] milliseconds period)
{
#ifdef _WIN32
	if (timeBeginPeriod(period.count()) != TIMERR_NOERROR)
		throw runtime_error{"Failed to initialize thread scheduler period"};
#endif
}

void end_scheduler_period([[maybe_unused]] milliseconds period) noexcept
{
#ifdef _WIN32
	timeEndPeriod(period.count());
#endif
}

void block_with_message(string_view message)
{
#ifdef _WIN32
	MessageBox(nullptr, string{message}.c_str(), AppTitle, MB_OK);
#endif
}

}

/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/thread.cpp:
Implementation file for lib/thread.hpp.
*/

#include "lib/os.hpp"
#include <sched.h>

#include "utils/config.hpp"
#ifdef TARGET_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <timeapi.h>
#include <shellapi.h>
#elifdef TARGET_LINUX
#include <linux/ioprio.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <unistd.h>
#endif
#include "preamble.hpp"

namespace playnote::lib::os {

void name_current_thread(string_view name)
{
#ifdef TARGET_WINDOWS
	auto const lname = std::wstring{name.begin(), name.end()}; // No reencoding; not expecting non-ASCII here
	auto const err = SetThreadDescription(GetCurrentThread(), lname.c_str());
	if (FAILED(err))
		throw runtime_error_fmt("Failed to set thread name: error {}", err);
#elifdef TARGET_LINUX
	auto const err = pthread_setname_np(pthread_self(), string{name}.c_str());
	if (err != 0)
		throw system_error("Failed to set thread name");
#endif
}

void lower_current_thread_priority()
{
#ifdef TARGET_WINDOWS
	SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
#elifdef TARGET_LINUX
	auto param = sched_param{};
	if (pthread_setschedparam(pthread_self(), SCHED_IDLE, &param) != 0 ||
		syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, 0, IOPRIO_PRIO_VALUE(IOPRIO_CLASS_IDLE, 0)) != 0)
		throw system_error("Failed to lower thread priority");
#endif
}

void begin_scheduler_period([[maybe_unused]] milliseconds period)
{
#ifdef TARGET_WINDOWS
	if (timeBeginPeriod(period.count()) != TIMERR_NOERROR)
		throw runtime_error{"Failed to initialize thread scheduler period"};
#endif
}

void end_scheduler_period([[maybe_unused]] milliseconds period) noexcept
{
#ifdef TARGET_WINDOWS
	timeEndPeriod(period.count());
#endif
}

void block_with_message([[maybe_unused]] string_view message)
{
#ifdef TARGET_WINDOWS
	MessageBoxA(nullptr, string{message}.c_str(), AppTitle, MB_OK);
#endif
}

}

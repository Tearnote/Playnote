#include "sys/os.hpp"

#include <string_view>
#include "config.hpp"

#if TARGET == TARGET_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <string>
#elif TARGET == TARGET_LINUX
#include <system_error>
#include <string>
#include <pthread.h>
#endif

SchedulerPeriod::SchedulerPeriod(nanoseconds period):
	period(period)
{
#if TARGET == TARGET_WINDOWS
	if (timeBeginPeriod(duration_cast<milliseconds>(period).count()) != TIMERR_NOERROR)
		throw std::runtime_error("Failed to initialize Windows scheduler period");
#endif
}

SchedulerPeriod::~SchedulerPeriod()
{
#if TARGET == TARGET_WINDOWS
	if (period == -1ns) return;
	timeEndPeriod(duration_cast<milliseconds>(period).count());
#endif
}

void set_thread_name(std::string_view name)
{
#ifdef THREAD_DEBUG
#if TARGET == TARGET_WINDOWS
	auto lname = std::wstring{name.begin(), name.end()};
	SetThreadDescription(GetCurrentThread(), lname.c_str());
#elif TARGET == TARGET_LINUX
	if (auto err = pthread_setname_np(pthread_self(), std::string(name).c_str()); err != 0)
		throw std::system_error(err, std::system_category(), "Failed to set thread name");
#endif
#endif
}

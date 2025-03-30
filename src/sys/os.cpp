#include "sys/os.hpp"

#include <string_view>
#include "config.hpp"

#if TARGET == TARGET_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif //NOMINMAX
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif //WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#elif TARGET == TARGET_LINUX
#include <system_error>
#include <string>
#include <pthread.h>
#endif

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

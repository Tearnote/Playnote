/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

dev/os.hpp:
Various OS-specific utilities.
*/

#pragma once
#include "preamble.hpp"
#include "config.hpp"
#include "lib/os.hpp"

namespace playnote::dev {

// Sets the system thread scheduler period for the lifetime of the instance.
// This decreases the minimum possible duration of thread sleep and yield.
class SchedulerPeriod {
public:
	explicit SchedulerPeriod(milliseconds period):
		period{period}
	{
		lib::os::begin_scheduler_period(period);
	}

	~SchedulerPeriod() noexcept { lib::os::end_scheduler_period(period); }

	SchedulerPeriod(SchedulerPeriod const&) = delete;
	auto operator=(SchedulerPeriod const&) -> SchedulerPeriod& = delete;
	SchedulerPeriod(SchedulerPeriod&&) = delete;
	auto operator=(SchedulerPeriod&&) -> SchedulerPeriod& = delete;

private:
	milliseconds period;
};

// Name the current thread. This name is visible in debuggers and profilers.
inline void name_current_thread(string_view name)
{
	lib::os::name_current_thread(name);
}

// Communicate a critical pre-init error to the user.
// Shows a message box on Windows, and prints to stderr on Linux.
template <typename... Args>
void syserror(format_string<Args...> fmt, Args&&... args)
{
#ifdef TARGET_WINDOWS
	lib::os::block_with_message(format(fmt, forward<Args>(args)...));
#elifdef TARGET_LINUX
	print(stderr, fmt, forward<Args>(args)...);
#endif
}

}

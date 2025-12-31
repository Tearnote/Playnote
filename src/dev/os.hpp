/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "utils/config.hpp"
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

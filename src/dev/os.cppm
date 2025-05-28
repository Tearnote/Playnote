/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

dev/os.cppm:
Various OS-specific utilities.
*/

export module playnote.dev.os;

import playnote.preamble;
import playnote.config;
import playnote.lib.thread;
import playnote.lib.tracy;

namespace playnote::dev {

namespace thread = lib::thread;
namespace tracy = lib::tracy;

// Sets the system thread scheduler period for the lifetime of the instance.
// This decreases the minimum possible duration of thread sleep and yield.
export class SchedulerPeriod {
public:
	explicit SchedulerPeriod(milliseconds period):
		period{period}
	{
		thread::begin_scheduler_period(period);
	}

	~SchedulerPeriod() noexcept { thread::end_scheduler_period(period); }

	SchedulerPeriod(SchedulerPeriod const&) = delete;
	auto operator=(SchedulerPeriod const&) -> SchedulerPeriod& = delete;
	SchedulerPeriod(SchedulerPeriod&&) = delete;
	auto operator=(SchedulerPeriod&&) -> SchedulerPeriod& = delete;

private:
	milliseconds period;
};

// Name the current thread. This name is visible in debuggers and profilers.
export void name_current_thread(string_view name)
{
	if constexpr (!ThreadNamesEnabled) return;
	thread::name_current(name);
	tracy::name_current_thread(name);
}

}

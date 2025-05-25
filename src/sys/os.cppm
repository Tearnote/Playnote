/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

sys/os.cppm:
Various OS-specific utilities.
*/

export module playnote.sys.os;

import playnote.preamble;
import playnote.config;
import playnote.lib.tracing;
import playnote.lib.thread;

namespace playnote::sys {

// Sets the system thread scheduler period for the lifetime of the instance.
// This decreases the minimum possible duration of thread sleep and yield.
export class SchedulerPeriod {
public:
	explicit SchedulerPeriod(milliseconds period):
		period{period}
	{
		lib::begin_thread_scheduler_period(period);
	}

	~SchedulerPeriod() noexcept
	{
		lib::end_thread_scheduler_period(period);
	}

	SchedulerPeriod(SchedulerPeriod const&) = delete;
	auto operator=(SchedulerPeriod const&) -> SchedulerPeriod& = delete;
	SchedulerPeriod(SchedulerPeriod&&) = delete;
	auto operator=(SchedulerPeriod&&) -> SchedulerPeriod& = delete;

private:
	milliseconds period;
};

// Name the current thread
export void set_thread_name(string_view name)
{
	if constexpr (!ThreadNamesEnabled) return;
	lib::set_thread_name(name);
	lib::tracing_set_thread_name(name);
}

}

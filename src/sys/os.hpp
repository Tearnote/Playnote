#ifndef PLAYNOTE_SYS_OS_H
#define PLAYNOTE_SYS_OS_H

#include <string_view>
#include "util/time.hpp"

class SchedulerPeriod {
public:
	explicit SchedulerPeriod(nanoseconds period);

	~SchedulerPeriod();

	SchedulerPeriod(SchedulerPeriod&& other)
	{
		*this = std::move(other);
	}

	auto operator=(SchedulerPeriod&& other) -> SchedulerPeriod&
	{
		period = other.period;
		other.period = -1ns;
		return *this;
	}

	SchedulerPeriod(SchedulerPeriod const&) = delete;
	auto operator=(SchedulerPeriod const&) -> SchedulerPeriod& = delete;

private:
	nanoseconds period;
};

void set_thread_name(std::string_view name);

#endif

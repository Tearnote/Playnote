#pragma once

#include <string_view>
#include "util/time.hpp"

class SchedulerPeriod
{
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

private:
	nanoseconds period;
};

void set_thread_name(std::string_view name);

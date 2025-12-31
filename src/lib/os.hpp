/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"

namespace playnote::lib::os {

// Confirm if mimalloc is loaded and active. In case of detected issues, logs a warning.
void check_mimalloc();

// Set current thread name in the OS scheduler, which can help with debugging.
// Throws runtime_error on failure.
void name_current_thread(string_view name);

// Lower a thread's priority level, in preparation of running intensive background jobs.
void lower_current_thread_priority();

// Set the thread scheduler period to at most the provided value. This can increase the resolution
// of thread sleep and yield. Pair with a matching callto end_thread_scheduler_period
// with the same period.
// Throws runtime_error on failure.
void begin_scheduler_period([[maybe_unused]] milliseconds period);

// End a previously started thread scheduler period. Failure is ignored.
void end_scheduler_period([[maybe_unused]] milliseconds period) noexcept;

// Block the current thread with a user-visible message box. Intended for early critical errors.
// Windows-only; on Linux use stderr output, as the console is always available there.
void block_with_message(string_view message);

// OS subpixel layout setting value.
enum class SubpixelLayout {
	None,
	HorizontalRGB,
	HorizontalBGR,
	VerticalRGB,
	VerticalBGR,
	Unknown,
};

// Retrieve the OS-wide subpixel layout setting.
auto get_subpixel_layout() -> SubpixelLayout;

}

/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/thread.hpp:
Wrapper for OS-specific thread handling functions not provided by STL.
*/

#pragma once
#include "preamble.hpp"

namespace playnote::lib::thread {

// Set current thread name in the OS scheduler, which can help with debugging.
// Throws runtime_error on failure.
void name_current(string_view name);

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

}

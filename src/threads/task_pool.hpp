/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/task_pool.hpp:
The global coroutine executor, implemented as a thread pool.
*/

#pragma once
#include "preamble.hpp"
#include "service.hpp"
#include "lib/coro.hpp"

namespace playnote::globals {
inline auto task_pool = Service<shared_ptr<coro::thread_pool>>{};
inline auto pool() -> coro::thread_pool& { return **task_pool; }
}

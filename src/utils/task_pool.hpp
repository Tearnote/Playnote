/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "utils/service.hpp"

namespace playnote::globals {
inline auto fg_pool = Service<unique_ptr<thread_pool>>{};
inline auto bg_pool = Service<unique_ptr<thread_pool>>{};
}

namespace playnote {

// Launch a fire-and-forget task on a thread pool.
inline void launch_task_on(unique_ptr<thread_pool>& pool, task<>&& t)
{
	pool->spawn(move(t));
}

// Schedule a task on the thread pool. The task will execute once the returned task is awaited.
template<typename T>
auto schedule_task_on(unique_ptr<thread_pool>& pool, task<T>&& t) -> task<T>
{
	return pool->schedule(move(t));
}

// Launch a task on the thread pool and return a future to its result, so that its result can be polled synchronously.
template<typename T>
auto launch_pollable_on(unique_ptr<thread_pool>& pool, task<T>&& t) -> future<T> {
	auto result_promise = promise<T>{};
	auto result_future = result_promise.get_future();
	launch_task_on(pool, [](promise<T> p, task<T> t) -> task<> {
		try {
			p.set_value(move(co_await t));
		}
		catch (...) {
			p.set_exception(current_exception());
		}
	}(move(result_promise), move(t)));
	return result_future;
}

// Shorthands for global pools

inline void launch_fg(task<>&& t) { launch_task_on(*globals::fg_pool, forward<task<>>(t)); }
inline void launch_bg(task<>&& t) { launch_task_on(*globals::bg_pool, forward<task<>>(t)); }
template<typename T>
auto schedule_fg(task<T>&& t) -> task<T> { return schedule_task_on(*globals::fg_pool, forward<task<T>>(t)); }
template<typename T>
auto schedule_bg(task<T>&& t) -> task<T> { return schedule_task_on(*globals::bg_pool, forward<task<T>>(t)); }
template<typename T>
auto pollable_fg(task<T>&& t) -> future<T> { return launch_pollable_on(*globals::fg_pool, forward<task<T>>(t)); }
template<typename T>
auto pollable_bg(task<T>&& t) -> future<T> { return launch_pollable_on(*globals::bg_pool, forward<task<T>>(t)); }

}

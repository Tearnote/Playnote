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
inline auto task_pool = Service<unique_ptr<thread_pool>>{};
}

namespace playnote {

// Launch a fire-and-forget task on the thread pool.
inline void launch_task(task<>&& t)
{
	(*globals::task_pool)->spawn(move(t));
}

// Schedule a task on the thread pool. The task will execute once the returned task is awaited.
template<typename T>
auto schedule_task(task<T>&& t) -> task<T>
{
	return (*globals::task_pool)->schedule(move(t));
}

// Launch a task on the thread pool and return a future to its result, so that its result can be polled synchronously.
template<typename T>
auto launch_pollable(task<T>&& t) -> future<T> {
	auto result_promise = promise<T>{};
	auto result_future = result_promise.get_future();
	launch_task([](promise<T> p, task<T> t) -> task<> {
		try {
			p.set_value(move(co_await t));
		}
		catch (...) {
			p.set_exception(current_exception());
		}
	}(move(result_promise), move(t)));
	return result_future;
}

}

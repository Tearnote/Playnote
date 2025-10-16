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
inline auto task_pool = Service<unique_ptr<coro::thread_pool>>{};
inline auto pool() -> coro::thread_pool& { return **task_pool; }
}

namespace playnote::threads {

// Launch a task on the thread pool and return a future to its result, so that its result can be polled synchronously.
template<typename T>
auto launch_pollable(coro::task<T>&& task) -> future<T> {
	auto result_promise = promise<T>{};
	auto result_future = result_promise.get_future();
	globals::pool().spawn([](promise<T> p, coro::task<T> t) -> coro::task<> {
		try {
			p.set_value(move(co_await t));
		}
		catch (...) {
			p.set_exception(current_exception());
		}
	}(move(result_promise), move(task)));
	return result_future;
}

}

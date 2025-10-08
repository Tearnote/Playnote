/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/task_queue.hpp:
A thread pool for executing tasks in the background and in parallel.
*/

#pragma once
#include "preamble.hpp"
#include "service.hpp"
#include "lib/mpmc.hpp"

namespace playnote::threads {

class TaskQueue {
public:
	template<typename T, typename P = void>
	class Task;

	TaskQueue();
	~TaskQueue();

	// Call the provided function inside the thread pool. A Task object is returned, which can be used
	// to retrieve the return value of the function once it's complete, or the exception if the function throws.
	template<typename... Args, invocable<Args...> Func>
	auto enqueue(Func&& func, Args&&... args) -> Task<invoke_result_t<Func, Args...>>;

	// An overload for functions that report progress through a channel.
	template<typename ProgressT, typename... Args, invocable<Args..., channel<ProgressT>&> Func>
	auto enqueue(Func&& func, Args&&... args) -> Task<invoke_result_t<Func, Args..., channel<ProgressT>&>, ProgressT>;

	TaskQueue(TaskQueue const&) = delete;
	auto operator=(TaskQueue const&) -> TaskQueue& = delete;
	TaskQueue(TaskQueue&&) = delete;
	auto operator=(TaskQueue&&) -> TaskQueue& = delete;

	template<typename T, typename P>
	class Task {
	public:
		// Retrieve the return value or exception by blocking until the task is complete.
		auto get() -> T { return fut.get(); }

		// Check if the task is complete.
		[[nodiscard]] auto is_ready() const -> bool { return fut.wait_for(0s) == future_status::ready; }

		// Call the provided function on every pending progress update.
		template<callable<void(P&&)> Func>
			requires (!same_as<P, void>)
		void on_progress(Func&&);

	private:
		friend class TaskQueue;
		future<T> fut;
		shared_ptr<channel<P>> progress;
	};

private:
	atomic<bool> running = true;
	vector<jthread> threads;
	lib::mpmc::Queue<move_only_function<void()>> queue;
};

template<typename... Args, invocable<Args...> Func>
auto TaskQueue::enqueue(Func&& func, Args&&... args) -> Task<invoke_result_t<Func, Args...>>
{
	using Ret = invoke_result_t<Func, Args...>;

	auto ret_promise = promise<Ret>{};
	auto task_stub = Task<Ret>{};
	task_stub.fut = ret_promise.get_future();
	queue.enqueue([
		func = decay_t<Func>(func),
		args = tuple(decay_t<Args>(args)...),
		ret_promise = move(ret_promise)
	] mutable {
		try {
			if constexpr (is_void_v<Ret>) {
				apply(func, move(args));
				ret_promise.set_value();
			} else {
				ret_promise.set_value(apply(func, move(args)));
			}
		} catch (...) {
			ret_promise.set_exception(current_exception());
		}
	});
	return task_stub;
}

template<typename ProgressT, typename... Args, invocable<Args..., channel<ProgressT>&> Func>
auto TaskQueue::enqueue(Func&& func, Args&&... args) -> Task<invoke_result_t<Func, Args..., channel<ProgressT>&>, ProgressT>
{
	using Ret = invoke_result_t<Func, Args..., channel<ProgressT>&>;

	auto ret_promise = promise<Ret>{};
	auto task_stub = Task<Ret, ProgressT>{};
	task_stub.fut = ret_promise.get_future();
	task_stub.progress = make_shared<channel<ProgressT>>();
	queue.enqueue([
		func = decay_t<Func>(func),
		args = tuple(decay_t<Args>(args)...),
		ret_promise = move(ret_promise),
		progress = task_stub.progress
	] mutable {
		try {
			auto final_args = tuple_cat(move(args), make_tuple(ref(*progress)));
			if constexpr (is_void_v<Ret>) {
				apply(func, move(final_args));
				ret_promise.set_value();
			} else {
				ret_promise.set_value(apply(func, move(final_args)));
			}
		} catch (...) {
			ret_promise.set_exception(current_exception());
		}
	});
	return task_stub;
}

template<typename T, typename P>
template<callable<void(P&&)> Func>
	requires (!same_as<P, void>)
void TaskQueue::Task<T, P>::on_progress(Func&& func)
{
	if (progress->empty()) return;
	for (auto&& message: *progress) {
		func(move(message));
		if (progress->empty()) return;
	}
}

inline TaskQueue::TaskQueue()
{
	static auto const ThreadCount = jthread::hardware_concurrency();
	auto thread_func = [this] {
		while (running) {
			auto task = move_only_function<void()>{};
			if (queue.try_dequeue(task)) task(); else yield();
		}
	};
	threads.reserve(ThreadCount);
	for (auto _: views::iota(0u, ThreadCount))
		threads.emplace_back(thread_func);
}

inline TaskQueue::~TaskQueue()
{
	running.store(false);
	// Destructor will wait for all threads to finish.
}

}

namespace playnote::globals {
inline auto task_queue = Service<threads::TaskQueue>{};
}

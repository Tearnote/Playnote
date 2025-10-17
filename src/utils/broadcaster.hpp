/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/broadcaster.hpp:
Allows threads to subscribe to events and receive messages from other threads.
*/

#pragma once
#include "preamble.hpp"
#include "utils/assert.hpp"

namespace playnote {

// Simple shared struct for controlling thread lifetime.
template<isize N>
struct Barriers {
	latch startup{N}; // Threads wait on this after registering with the broadcaster
	latch shutdown{N}; // Threads wait on this before exiting
};

class Broadcaster {
public:
	Broadcaster() = default;

	// Declare that the current thread will send and/or receive messages.
	void register_as_endpoint();

	// Declare that current thread is interested in messages of type T.
	template<typename T>
	void subscribe();

	// Send message to all other threads that declared interest in this type.
	template<typename T>
	void shout(T&& message) { make_shout<T>(forward<T>(message)); }

	// Send message to all other threads that declared interest in this type, constructing
	// the message in-place.
	template<typename T, typename... Args>
	void make_shout(Args&&... args);

	// Call the provided function once for every pending message of type T. Must have previously
	// subscribed to this type. Returns true if at least one message was processed.
	template<typename T, callable<void(T&&)> Func>
	auto receive_all(Func&& func) -> bool;

	// Block until a message of type T is received. The sleep callback is called repeatedly until
	// the message arrives. Function might be called more than once if multiple messages were sent
	// in rapid succession. Be careful, as there's no timeout. Must have previously subscribed
	// to this type.
	template<typename T, callable<void(T&&)> Func, callable<void()> SleepFunc>
	void await(Func&& func, SleepFunc&& sleep);

private:
	inline static thread_local auto endpoint_id = -1z;
	mutex register_lock;
	vector<unordered_map<type_index, shared_ptr<void>>> queues;
};

inline void Broadcaster::register_as_endpoint()
{
	ASSUME(endpoint_id == -1z);
	auto lock = lock_guard{register_lock};
	endpoint_id = queues.size();
	queues.emplace_back();
}

template<typename T>
void Broadcaster::subscribe()
{
	using Type = remove_cvref_t<T>;
	ASSUME(endpoint_id != -1z);
	auto lock = lock_guard{register_lock};
	ASSUME(!queues[endpoint_id].contains(typeid(Type)));
	queues[endpoint_id][typeid(Type)] = make_shared<mpmc_queue<Type>>();
}

template<typename T, typename... Args>
void Broadcaster::make_shout(Args&&... args)
{
	using Type = remove_cvref_t<T>;
	ASSUME(endpoint_id != -1z);
	for (auto [idx, in_channel]: queues | views::enumerate) {
		if (endpoint_id == idx) continue;
		if (!in_channel.contains(typeid(Type))) continue;
		(*static_pointer_cast<mpmc_queue<Type>>(in_channel[typeid(Type)])).enqueue(T{forward<Args>(args)...});
	}
}

template<typename T, callable<void(T&&)> Func>
auto Broadcaster::receive_all(Func&& func) -> bool
{
	using Type = remove_cvref_t<T>;
	ASSUME(endpoint_id != -1z);
	ASSUME(queues[endpoint_id].contains(typeid(Type)));
	auto& out_queue = *static_pointer_cast<mpmc_queue<Type>>(queues[endpoint_id][typeid(Type)]);
	auto message = Type{};
	auto processed = 0z;
	while (out_queue.try_dequeue(message)) {
		func(move(message));
		processed += 1;
	}
	return processed;
}

template<typename T, callable<void(T&&)> Func, callable<void()> SleepFunc>
void Broadcaster::await(Func&& func, SleepFunc&& sleep)
{
	while (!receive_all<T>(forward<Func>(func))) sleep();
}

}

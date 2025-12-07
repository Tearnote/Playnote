/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "utils/assert.hpp"

namespace playnote {

// Simple shared struct for controlling thread lifetime.
template<ssize_t N>
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

	// Return all pending messages of type T. Must have previously subscribed to this type.
	template<typename T>
	auto receive_all() -> generator<T>;

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

template<typename T>
auto Broadcaster::receive_all() -> generator<T>
{
	using Type = remove_cvref_t<T>;
	ASSUME(endpoint_id != -1z);
	ASSUME(queues[endpoint_id].contains(typeid(Type)));
	auto& out_queue = *static_pointer_cast<mpmc_queue<Type>>(queues[endpoint_id][typeid(Type)]);
	auto message = Type{};
	while (out_queue.try_dequeue(message))
		co_yield move(message);
}

}

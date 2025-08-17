/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/broadcaster.hpp:
Allows threads to subscribe to events and receive messages from other threads.
*/

#pragma once
#include "preamble.hpp"
#include "assert.hpp"

namespace playnote::threads {

// Simple shared struct for controlling thread lifetime.
template<usize N>
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
	inline static thread_local auto endpoint_id = -1zu;
	mutex register_lock;
	vector<unordered_map<type_index, shared_ptr<void>>> channels;
};

inline void Broadcaster::register_as_endpoint()
{
	ASSUME(endpoint_id == -1zu);
	auto lock = lock_guard{register_lock};
	endpoint_id = channels.size();
	channels.emplace_back();
}

template<typename T>
void Broadcaster::subscribe()
{
	using Type = remove_cvref_t<T>;
	ASSUME(endpoint_id != -1zu);
	auto lock = lock_guard{register_lock};
	ASSUME(!channels[endpoint_id].contains(typeid(Type)));
	channels[endpoint_id][typeid(Type)] = make_shared<channel<Type>>();
}

template<typename T, typename... Args>
void Broadcaster::make_shout(Args&&... args)
{
	using Type = remove_cvref_t<T>;
	ASSUME(endpoint_id != -1zu);
	for (auto idx: irange(0zu, channels.size())) {
		if (endpoint_id == idx) continue;
		auto& in_channel = channels[idx];
		if (!in_channel.contains(typeid(Type))) continue;
		(*static_pointer_cast<channel<Type>>(in_channel[typeid(Type)])) << T{forward<Args>(args)...};
	}
}

template<typename T, callable<void(T&&)> Func>
auto Broadcaster::receive_all(Func&& func) -> bool
{
	using Type = remove_cvref_t<T>;
	ASSUME(endpoint_id != -1zu);
	ASSUME(channels[endpoint_id].contains(typeid(Type)));
	auto& out_channel = *static_pointer_cast<channel<Type>>(channels[endpoint_id][typeid(Type)]);
	if (out_channel.empty()) return false;
	for (auto message: out_channel) {
		func(move(message));
		if (out_channel.empty()) return true;
	}
	return false;
}

template<typename T, callable<void(T&&)> Func, callable<void()> SleepFunc>
void Broadcaster::await(Func&& func, SleepFunc&& sleep)
{
	while (!receive_all<T>(forward<Func>(func))) sleep();
}

}

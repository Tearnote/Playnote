/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/broadcaster.cppm:
Allows threads to subscribe to events and receive messages from other threads.
*/

module;
#include "macros/assert.hpp"
#include "quill/core/Codec.h"

export module playnote.threads.broadcaster;

import playnote.preamble;

namespace playnote::threads {

export class Broadcaster {
public:
	Broadcaster() = default;

	// Declare that the current thread will send and/or receive messages.
	void register_as_endpoint();

	// Declare that current thread is interested in messages of type T.
	template<typename T>
	void subscribe();

	// Blocks current thread until all other threads have called wait_for_others() as well.
	// All threads need to pass the same value of thread_count which is equal to the number
	// of threads that called this function. This can only be used once per thread.
	void wait_for_others(uint32 thread_count);

	// Send message to all other threads that declared interest in this type.
	template<typename T>
	void shout(T&& message);

	// Call the provided function once for every pending message of type T. Must have previously
	// subscribed to this type. Returns true if at least one message was processed.
	template<typename T, callable<void(T&&)> Func>
	auto receive_all(Func&& func) -> bool;

private:
	inline static thread_local auto endpoint_id = -1zu;
	optional<latch> startup_sync;
	mutex register_lock;
	vector<unordered_map<type_index, shared_ptr<void>>> channels;
};

void Broadcaster::register_as_endpoint()
{
	ASSUME(endpoint_id == -1zu);
	auto lock = lock_guard{register_lock};
	endpoint_id = channels.size();
	channels.emplace_back();
}

void Broadcaster::wait_for_others(uint32 thread_count)
{
	if (!startup_sync) startup_sync.emplace(thread_count);
	startup_sync->arrive_and_wait();
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

template<typename T>
void Broadcaster::shout(T&& message)
{
	using Type = remove_cvref_t<T>;
	ASSUME(endpoint_id != -1zu);
	for (auto& in_channel: channels) {
		if (&in_channel - &channels.front() == endpoint_id) continue;
		if (!in_channel.contains(typeid(Type))) continue;
		(*static_pointer_cast<channel<Type>>(in_channel[typeid(Type)])) << message;
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
}

}

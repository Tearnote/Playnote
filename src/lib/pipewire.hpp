/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/pipewire.hpp:
Wrapper for libpipewire client library for Linux audio support.
*/

#pragma once
#include "preamble.hpp"

// Forward declarations

struct pw_thread_loop;
struct pw_stream;
struct spa_pod;
struct pw_buffer;

namespace playnote::lib::pw {
// Initialize PipeWire.
void init();

// Return the runtime version of PipeWire.
[[nodiscard]] auto get_version() -> string_view;

using ThreadLoop = pw_thread_loop*;

// Create a new thread loop object.
// Throws system_error on failure.
[[nodiscard]] auto create_thread_loop() -> ThreadLoop;

// Destroy the thread loop.
void destroy_thread_loop(ThreadLoop loop) noexcept;

// Start the thread loop. It will begin to process events on its own thread.
void start_thread_loop(ThreadLoop loop);

// Lock the thread loop. This ensures that succeeding code won't run concurrently with the callback.
void lock_thread_loop(ThreadLoop loop);

// Unlock the thread loop, allowing the callbacks to run again.
void unlock_thread_loop(ThreadLoop loop);

struct Stream_t;

using Stream = Stream_t*;

using SPAPod = spa_pod const*;
using ProcessCallback = void(*)(void*);
using ParamChangedCallback = void(*)(void*, uint32_t, SPAPod);

// Helper function to extract a new sampling rate that was set for the stream. If the event is about
// something else, returns nullopt.
auto get_sampling_rate_from_param(uint32_t id, spa_pod const* param) -> optional<uint32>;

namespace detail {

auto create_stream_raw(ThreadLoop loop, string_view name, uint32 latency,
	ProcessCallback on_process, ParamChangedCallback on_param_changed, void* user_ptr) -> Stream;

}

// Create a new audio stream with the specified parameters. Sampling rate is in samples per second,
// latency is in samples. The stream will only begin processing once the loop is started.
// The on_process function will be called on the runtime-priority thread every time new audio needs
// to be provided, and it will receive the optional user_ptr as argument. The stream will have
// 2 audio channels and 32-bit float sample format.
// Throws system_error on failure.
template<typename T = void>
[[nodiscard]] auto create_stream(ThreadLoop loop, string_view name, uint32 latency,
	ProcessCallback on_process, ParamChangedCallback on_param_changed, T* user_ptr = nullptr) -> Stream
{
	return detail::create_stream_raw(loop, name, latency, on_process, on_param_changed, user_ptr);
}

// Destroy an audio stream. Ensures the loop is frozen for the duration to prevent race conditions
// with the realtime thread.
void destroy_stream(ThreadLoop loop, Stream stream) noexcept;

// Return the playback progress of the stream.
auto get_stream_time(Stream const& stream) noexcept -> nanoseconds;

// A single audio sample (frame).
struct Sample {
	float left;
	float right;
};

using BufferRequest = pw_buffer*;

// Retrieve a buffer request from the queue. If return value is nullopt, a buffer is unavailable,
// and there is nothing to do. Otherwise, return value is the buffer which needs to be filled
// to its full size, and the request object to submit back when finished.
[[nodiscard]] auto dequeue_buffer(Stream& stream) -> optional<pair<span<Sample>, BufferRequest>>;

// Submit a fulfilled buffer request.
void enqueue_buffer(Stream& stream, BufferRequest request);

}

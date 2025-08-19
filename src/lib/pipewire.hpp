/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/pipewire.hpp:
Wrapper for libpipewire client library for Linux audio support.
*/

#pragma once
#include "preamble.hpp"
#include "lib/audio_common.hpp"

// Forward declarations

struct pw_thread_loop;
struct pw_stream;
struct spa_pod;
struct pw_buffer;

namespace playnote::lib::pw {

// Forward declarations

struct Stream_t;

struct Context {
	AudioProperties properties;
	pw_thread_loop* loop;
	Stream_t* stream;
};

using SPAPod = spa_pod const*;
using ProcessCallback = void(*)(void*);
using ParamChangedCallback = void(*)(void*, uint32_t, SPAPod);

namespace detail {

auto init_raw(string_view stream_name, uint32 buffer_size,
	ProcessCallback on_process, ParamChangedCallback on_param_changed, void* user_ptr) -> Context;

}

// Initialize PipeWire and open an audio stream.
// Throws system_error on failure.
template<typename T = void>
[[nodiscard]] auto init(string_view stream_name, uint32 buffer_size, ProcessCallback on_process,
	ParamChangedCallback on_param_changed, T* user_ptr = nullptr) -> Context
{
	return detail::init_raw(stream_name, buffer_size, on_process, on_param_changed, user_ptr);
}

// Clean up PipeWire and associated objects.
void cleanup(Context&& context);

// Helper function to extract a new sampling rate that was set for the stream. If the event is about
// something else, returns nullopt.
auto get_sampling_rate_from_param(uint32_t id, spa_pod const* param) -> optional<uint32>;

using BufferRequest = pw_buffer*;

// Retrieve a buffer request from the queue. If return value is nullopt, a buffer is unavailable,
// and there is nothing to do. Otherwise, return value is the buffer which needs to be filled
// to its full size, and the request object to submit back when finished.
[[nodiscard]] auto dequeue_buffer(Context&) -> optional<pair<span<Sample>, BufferRequest>>;

// Submit a fulfilled buffer request.
void enqueue_buffer(Context&, BufferRequest);

}

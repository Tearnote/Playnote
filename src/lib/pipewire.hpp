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

struct Context_t {
	AudioProperties properties;
	pw_thread_loop* loop;
	Stream_t* stream;
	void* user_ptr;
};
using Context = unique_ptr<Context_t>;

using ProcessCallback = void(*)(void*);

// Initialize PipeWire and open an audio stream.
// Throws system_error on failure.
auto init(string_view stream_name, uint32 buffer_size, ProcessCallback on_process, void* user_ptr) -> Context;

// Clean up PipeWire and associated objects.
void cleanup(Context&& context);

using BufferRequest = pw_buffer*;

// Retrieve a buffer request from the queue. If return value is nullopt, a buffer is unavailable,
// and there is nothing to do. Otherwise, return value is the buffer which needs to be filled
// to its full size, and the request object to submit back when finished.
[[nodiscard]] auto dequeue_buffer(Context_t*) -> optional<pair<span<Sample>, BufferRequest>>;

// Submit a fulfilled buffer request.
void enqueue_buffer(Context_t*, BufferRequest);

}

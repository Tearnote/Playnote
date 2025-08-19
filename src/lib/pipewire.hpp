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
	function<void(span<Sample>)> processor;
};
using Context = unique_ptr<Context_t>;

// Initialize PipeWire and open an audio stream.
// Throws system_error on failure.
auto init(string_view stream_name, uint32 buffer_size, function<void(span<Sample>)>&& processor) -> Context;

// Clean up PipeWire and associated objects.
void cleanup(Context&& context);

}

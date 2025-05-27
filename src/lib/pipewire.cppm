/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/pipewire.cppm:
Wrapper for libpipewire client library for Linux audio support.
*/

module;
#include <spa/param/audio/format-utils.h>
#include <spa/param/latency-utils.h>
#include <pipewire/pipewire.h>
#include "macros/assert.hpp"

export module playnote.lib.pipewire;

import playnote.preamble;

namespace playnote::lib::pw {

// Error checking wrappers for libpipewire functions.
template<typename T>
auto ptr_check(T* ptr, string_view message = "libpipewire error") -> T*
{
	if (!ptr) throw system_error_fmt("{}", message);
	return ptr;
}

void ret_check(int ret, string_view message = "libpipewire error")
{
	if (ret < 0) throw system_error_fmt("{}", message);
}

// Initialize PipeWire.
export void init() noexcept { pw_init(nullptr, nullptr); }

// Return the runtime version of PipeWire.
export auto get_version() noexcept -> string_view { return ASSERT_VAL(pw_get_library_version()); }

export using ThreadLoop = pw_thread_loop*;

// Create a new thread loop object.
// Throws system_error on failure.
export auto create_thread_loop() -> ThreadLoop
{
	return ptr_check(pw_thread_loop_new(nullptr, nullptr));
}

// Destroy the thread loop.
export void destroy_thread_loop(ThreadLoop loop) noexcept { pw_thread_loop_destroy(loop); }

// Start the thread loop. It will begin to process events on its own thread.
export void start_thread_loop(ThreadLoop loop) noexcept { pw_thread_loop_start(loop); }

export using Stream = pw_stream*;
using ProcessCallback = void(*)(void*);

// Create a new audio stream with the specified parameters. Sampling rate is in samples per second,
// latency is in samples. The stream will only begin processing once the loop is started.
// The on_process function will be called on the runtime-priority thread every time new audio needs
// to be provided, and it will receive the optional user_ptr as argument. The stream will have
// 2 audio channels and 32-bit float sample format.
// Throws system_error on failure.
export template<typename T = void, typename = decltype([]{})> // Thumbprint ensures static data is unique per callsite
auto create_stream(ThreadLoop loop, string_view name, uint32 sampling_rate, uint32 latency, ProcessCallback on_process, T* user_ptr = nullptr) -> Stream
{
	static auto const StreamEvents = pw_stream_events{
		.version = PW_VERSION_STREAM_EVENTS,
		.process = on_process,
	};
	auto const latency_str = format("{}/{}", latency, sampling_rate);

	auto stream = pw_stream_new_simple(
		pw_thread_loop_get_loop(loop), string{name}.c_str(),
		pw_properties_new(
			PW_KEY_MEDIA_TYPE, "Audio",
			PW_KEY_MEDIA_CATEGORY, "Playback",
			PW_KEY_MEDIA_ROLE, "Game",
			PW_KEY_NODE_LATENCY, latency_str.c_str(),
		nullptr),
		&StreamEvents, user_ptr);

	auto params = array<spa_pod const*, 1>{};
	auto buffer = array<uint8, 1024>{};
	auto builder = SPA_POD_BUILDER_INIT(buffer.data(), buffer.size());
	auto const audio_info = spa_audio_info_raw{
		.format = SPA_AUDIO_FORMAT_F32,
		.rate = sampling_rate,
		.channels = 2,
	};
	params[0] = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &audio_info);
	ret_check(pw_stream_connect(stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
		static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS),
		params.data(), 1));

	return stream;
}

// Destroy the stream. Make sure it's not in use anymore first.
export void destroy_stream(ThreadLoop loop, Stream stream) noexcept
{
	pw_thread_loop_lock(loop);
	pw_stream_destroy(stream);
	pw_thread_loop_unlock(loop);
}

// A single audio sample (frame).
export struct Sample {
	float left;
	float right;
};

export using BufferRequest = pw_buffer*;

// Retrieve a buffer request from the queue. If return value is nullopt, a buffer is unavailable,
// and there is nothing to do. Otherwise, return value is the buffer which needs to be filled
// to its full size, and the request object to submit back when finished.
export auto dequeue_buffer(Stream stream) noexcept -> optional<pair<span<Sample>, BufferRequest>>
{
	auto* buffer_outer = pw_stream_dequeue_buffer(stream);
	if (!buffer_outer) return nullopt;
	auto* buffer = buffer_outer->buffer;
	auto* output = buffer->datas[0].data;
	if (!output) return nullopt;

	constexpr auto Stride = sizeof(float) * 2 /* channel count */;
	auto const max_frames = buffer->datas[0].maxsize / Stride;
	auto const frames = min(max_frames, buffer_outer->requested);

	buffer->datas[0].chunk->offset = 0;
	buffer->datas[0].chunk->stride = Stride;
	buffer->datas[0].chunk->size = frames * Stride;

	return make_pair(span{static_cast<Sample*>(output), frames}, buffer_outer);
}

// Submit a fulfilled buffer request.
export void enqueue_buffer(Stream stream, BufferRequest request) noexcept
{
	pw_stream_queue_buffer(stream, request);
}

}

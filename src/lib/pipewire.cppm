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
#include "preamble.hpp"
#include "logger.hpp"

export module playnote.lib.pipewire;

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
export void init() { pw_init(nullptr, nullptr); }

// Return the runtime version of PipeWire.
export [[nodiscard]] auto get_version() -> string_view { return ASSERT_VAL(pw_get_library_version()); }

export using ThreadLoop = pw_thread_loop*;

// Create a new thread loop object.
// Throws system_error on failure.
export [[nodiscard]] auto create_thread_loop() -> ThreadLoop
{
	return ptr_check(pw_thread_loop_new(nullptr, nullptr));
}

// Destroy the thread loop.
export void destroy_thread_loop(ThreadLoop loop) noexcept { pw_thread_loop_destroy(loop); }

// Start the thread loop. It will begin to process events on its own thread.
export void start_thread_loop(ThreadLoop loop) { pw_thread_loop_start(loop); }

// Lock the thread loop. This ensures that succeeding code won't run concurrently with the callback.
export void lock_thread_loop(ThreadLoop loop) { pw_thread_loop_lock(loop); }

// Unlock the thread loop, allowing the callbacks to run again.
export void unlock_thread_loop(ThreadLoop loop) { pw_thread_loop_unlock(loop); }

export using Stream = pw_stream*;
export using SPAPod = spa_pod const*;
using ProcessCallback = void(*)(void*);
using ParamChangedCallback = void(*)(void*, uint32_t, SPAPod);

// Helper function to extract a new sampling rate that was set for the stream. If the event is about
// something else, returns nullopt.
export auto get_sampling_rate_from_param(uint32_t id, spa_pod const* param) -> optional<uint32>
{
	if (!param || id != SPA_PARAM_Format) return nullopt;
	auto audio_info = spa_audio_info{};
	ret_check(spa_format_parse(param, &audio_info.media_type, &audio_info.media_subtype) < 0);
	if (audio_info.media_type != SPA_MEDIA_TYPE_audio || audio_info.media_subtype != SPA_MEDIA_SUBTYPE_raw) return nullopt;
	spa_format_audio_raw_parse(param, &audio_info.info.raw);
	return audio_info.info.raw.rate;
}

// Create a new audio stream with the specified parameters. Sampling rate is in samples per second,
// latency is in samples. The stream will only begin processing once the loop is started.
// The on_process function will be called on the runtime-priority thread every time new audio needs
// to be provided, and it will receive the optional user_ptr as argument. The stream will have
// 2 audio channels and 32-bit float sample format.
// Throws system_error on failure.
export template<typename T = void, typename = decltype([]{})> // Thumbprint ensures static data is unique per callsite
[[nodiscard]] auto create_stream(ThreadLoop loop, string_view name, uint32 latency,
	ProcessCallback on_process, ParamChangedCallback on_param_changed, T* user_ptr = nullptr) -> Stream
{
	static auto const StreamEvents = pw_stream_events{
		.version = PW_VERSION_STREAM_EVENTS,
		.param_changed = on_param_changed,
		.process = on_process,
	};
	auto stream = pw_stream_new_simple(
		pw_thread_loop_get_loop(loop), string{name}.c_str(),
		pw_properties_new(
			PW_KEY_MEDIA_TYPE, "Audio",
			PW_KEY_MEDIA_CATEGORY, "Playback",
			PW_KEY_MEDIA_ROLE, "Game",
			PW_KEY_NODE_FORCE_QUANTUM, format("{}", latency).c_str(),
		nullptr),
		&StreamEvents, user_ptr);

	auto params = array<spa_pod const*, 1>{};
	auto buffer = array<uint8, 1024>{};
	auto builder = SPA_POD_BUILDER_INIT(buffer.data(), buffer.size());
	constexpr auto audio_info = spa_audio_info_raw{
		.format = SPA_AUDIO_FORMAT_F32,
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

export auto get_stream_time(Stream stream) -> nanoseconds
{
	return nanoseconds{pw_stream_get_nsec(stream)};
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
export [[nodiscard]] auto dequeue_buffer(Stream stream) -> optional<pair<span<Sample>, BufferRequest>>
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
	fill(span{static_cast<byte*>(output), frames * Stride}, static_cast<byte>(0));

	return make_pair(span{static_cast<Sample*>(output), frames}, buffer_outer);
}

// Submit a fulfilled buffer request.
export void enqueue_buffer(Stream stream, BufferRequest request)
{
	pw_stream_queue_buffer(stream, request);
}

}

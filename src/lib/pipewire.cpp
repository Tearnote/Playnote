/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/pipewire.cpp:
Implementation file for lib/pipewire.hpp.
*/

#include "lib/pipewire.hpp"

#include <spa/param/audio/format-utils.h>
#include <spa/param/latency-utils.h>
#include <pipewire/pipewire.h>
#include "preamble.hpp"
#include "assert.hpp"
#include "logger.hpp"

namespace playnote::lib::pw {

// Helper functions for error handling

template<typename T>
static auto ptr_check(T* ptr, string_view message = "libpipewire error") -> T*
{
	if (!ptr) throw system_error_fmt("{}", message);
	return ptr;
}

static void ret_check(int ret, string_view message = "libpipewire error")
{
	if (ret < 0) throw system_error_fmt("{}", message);
}

[[nodiscard]] auto get_version() -> string_view { return ASSERT_VAL(pw_get_library_version()); }

auto get_sampling_rate_from_param(uint32_t id, spa_pod const* param) -> optional<uint32>
{
	if (!param || id != SPA_PARAM_Format) return nullopt;
	auto audio_info = spa_audio_info{};
	ret_check(spa_format_parse(param, &audio_info.media_type, &audio_info.media_subtype) < 0);
	if (audio_info.media_type != SPA_MEDIA_TYPE_audio || audio_info.media_subtype != SPA_MEDIA_SUBTYPE_raw) return nullopt;
	spa_format_audio_raw_parse(param, &audio_info.info.raw);
	return audio_info.info.raw.rate;
}

struct Stream_t {
	pw_stream* stream;
	pw_stream_events events;
};

namespace detail {

[[nodiscard]] auto init_raw(string_view stream_name, uint32 buffer_size, ProcessCallback on_process,
	ParamChangedCallback on_param_changed, void* user_ptr) -> Context
{
	pw_init(nullptr, nullptr);
	auto loop = ptr_check(pw_thread_loop_new(nullptr, nullptr));

	auto const stream = new Stream_t{};
	stream->events = pw_stream_events{
		.version = PW_VERSION_STREAM_EVENTS,
		.param_changed = on_param_changed,
		.process = on_process,
	};
	stream->stream = pw_stream_new_simple(
		pw_thread_loop_get_loop(loop), string{stream_name}.c_str(),
		pw_properties_new(
			PW_KEY_MEDIA_TYPE, "Audio",
			PW_KEY_MEDIA_CATEGORY, "Playback",
			PW_KEY_MEDIA_ROLE, "Game",
			PW_KEY_NODE_FORCE_QUANTUM, format("{}", buffer_size).c_str(),
		nullptr),
		&stream->events, user_ptr);

	auto params = array<spa_pod const*, 1>{};
	auto buffer = array<uint8, 1024>{};
	auto builder = SPA_POD_BUILDER_INIT(buffer.data(), buffer.size());
	constexpr auto audio_info = spa_audio_info_raw{
		.format = SPA_AUDIO_FORMAT_F32,
		.channels = 2,
	};
	params[0] = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &audio_info);
	ret_check(pw_stream_connect(stream->stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
		static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS),
		params.data(), 1));

	pw_thread_loop_start(loop);

	return Context{
		.properties = AudioProperties{
			.sampling_rate = 0, // Unknown for now
			.sample_format = SampleFormat::Float32,
			.buffer_size = buffer_size,
		},
		.loop = loop,
		.stream = stream,
	};
}

}

void cleanup(Context&& context)
{
	pw_thread_loop_lock(context.loop);
	pw_stream_destroy(context.stream->stream);
	pw_thread_loop_unlock(context.loop);
	delete context.stream;
	pw_thread_loop_destroy(context.loop);
}

[[nodiscard]] auto dequeue_buffer(Context& ctx) -> optional<pair<span<Sample>, BufferRequest>>
{
	auto* buffer_outer = pw_stream_dequeue_buffer(ctx.stream->stream);
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

void enqueue_buffer(Context& ctx, BufferRequest request)
{
	pw_stream_queue_buffer(ctx.stream->stream, request);
}

}

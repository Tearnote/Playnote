module;
#include <string_view>
#include <algorithm>
#include <vector>
#include <thread>
#include <ranges>
#include <array>
#include <cmath>
#include <spa/param/audio/format-utils.h>
#include <spa/param/latency-utils.h>
#include <pipewire/pipewire.h>
#include "util/log_macros.hpp"
#include "config.hpp"
#include "quill/bundled/fmt/base.h"

export module playnote.sys.audio;

import playnote.stx.except;
import playnote.stx.types;
import playnote.stx.math;

namespace playnote::sys {

namespace ranges = std::ranges;
using stx::uint;
using stx::uint8;
using stx::usize;

export class Audio {
public:
	Audio(int argc, char* argv[]);
	~Audio();

	Audio(Audio const&) = delete;
	auto operator=(Audio const&) -> Audio& = delete;
	Audio(Audio&&) = delete;
	auto operator=(Audio&&) -> Audio& = delete;

private:
	static constexpr auto ChannelCount = 2u;
	static constexpr auto SamplingRate = 48000u;
	static constexpr auto Latency = "256/48000";

	pw_thread_loop* loop{nullptr};
	pw_stream* stream{nullptr};

	static void on_process(void*);
	inline static const auto StreamEvents = pw_stream_events{
		.version = PW_VERSION_STREAM_EVENTS,
		.process = on_process,
	};

	template<typename T>
	static auto ptr_check(T* ptr, std::string_view message = "libpipewire error") -> T*
	{
		if (!ptr) throw stx::system_error_fmt("{}", message);
		return ptr;
	}

	static void ret_check(int ret, std::string_view message = "libpipewire error")
	{
		if (ret < 0) throw stx::system_error_fmt("{}", message);
	}
};

Audio::Audio(int argc, char* argv[]) {
	pw_init(&argc, &argv);
	L_DEBUG("Using libpipewire {}\n", pw_get_library_version());

	loop = ptr_check(pw_thread_loop_new(nullptr, nullptr));
	stream = pw_stream_new_simple(
		pw_thread_loop_get_loop(loop), std::string{AppTitle}.c_str(),
		pw_properties_new(
			PW_KEY_MEDIA_TYPE, "Audio",
			PW_KEY_MEDIA_CATEGORY, "Playback",
			PW_KEY_MEDIA_ROLE, "Game",
			PW_KEY_NODE_LATENCY, Latency,
		nullptr),
		&StreamEvents, this);

	auto params = std::array<spa_pod const*, 1>{};
	auto buffer = std::array<uint8, 1024>{};
	auto builder = SPA_POD_BUILDER_INIT(buffer.data(), buffer.size());
	auto audio_info = spa_audio_info_raw{
		.format = SPA_AUDIO_FORMAT_F32,
		.rate = SamplingRate,
		.channels = ChannelCount,
	};
	params[0] = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &audio_info);
	ret_check(pw_stream_connect(stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
		static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS),
		params.data(), 1));

	pw_thread_loop_start(loop);
}

Audio::~Audio()
{
	pw_thread_loop_lock(loop);
	pw_stream_destroy(stream);
	pw_thread_loop_unlock(loop);
	pw_thread_loop_destroy(loop);
}

void Audio::on_process(void* userdata)
{
	auto& self = *static_cast<Audio*>(userdata);

	auto* buffer_outer = pw_stream_dequeue_buffer(self.stream);
	if (!buffer_outer) {
		L_WARN("Ran out of audio buffers");
		return;
	}
	auto* buffer = buffer_outer->buffer;
	auto* output = static_cast<float*>(buffer->datas[0].data);
	if (!output) return;

	constexpr auto Stride = sizeof(float) * ChannelCount;
	const auto max_frames = buffer->datas[0].maxsize / Stride;
	auto frames = std::min(max_frames, buffer_outer->requested);
/*
	if (!self.audios.empty()) {
		for (auto i: ranges::iota_view(0u, frames)) {
			auto frame = std::array<float, ChannelCount>{};
			for (auto& audio: self.audios) {
				if (self.audio_progress * 2 >= audio.size()) continue;
				frame[0] += audio[self.audio_progress * 2    ] / self.audios.size();
				frame[1] += audio[self.audio_progress * 2 + 1] / self.audios.size();
			}

			output[i * ChannelCount    ] = frame[0];
			output[i * ChannelCount + 1] = frame[1];
			self.audio_progress += 1;
		}
	}
*/
	buffer->datas[0].chunk->offset = 0;
	buffer->datas[0].chunk->stride = Stride;
	buffer->datas[0].chunk->size = frames * Stride;
	pw_stream_queue_buffer(self.stream, buffer_outer);
}

}

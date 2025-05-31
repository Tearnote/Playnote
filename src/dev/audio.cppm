/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

dev/audio.cppm:
Initializes audio and submits buffers for playback.
*/

module;
#include "macros/logger.hpp"

export module playnote.dev.audio;

import playnote.preamble;
import playnote.config;
import playnote.logger;
import playnote.lib.pipewire;
import playnote.bms.chart;

namespace playnote::dev {

namespace pw = lib::pw;

export class Audio {
public:
	Audio();
	~Audio();

	void play_chart(bms::Chart& chart);

	Audio(Audio const&) = delete;
	auto operator=(Audio const&) -> Audio& = delete;
	Audio(Audio&&) = delete;
	auto operator=(Audio&&) -> Audio& = delete;

private:
	static constexpr auto ChannelCount = 2u;
	static constexpr auto SamplingRate = 48000u;
	static constexpr auto Latency = 256u;

	InstanceLimit<Audio, 1> instance_limit;

	pw::ThreadLoop loop;
	pw::Stream stream;

	atomic<bool> chart_playback_started = false;
	optional<reference_wrapper<bms::Chart>> chart;
	nanoseconds chart_start_time;

	static void on_process(void*) noexcept;
};

Audio::Audio() {
	pw::init();
	DEBUG("Using libpipewire {}", pw::get_version());

	loop = pw::create_thread_loop();
	stream = pw::create_stream(loop, AppTitle, SamplingRate, Latency, &on_process, this);
	pw::start_thread_loop(loop);
}

Audio::~Audio()
{
	pw::destroy_stream(loop, stream);
	pw::destroy_thread_loop(loop);
}

void Audio::play_chart(bms::Chart& chart)
{
	this->chart = chart;
	chart_start_time = pw::get_stream_time(stream);
	chart_playback_started = true;
}

void Audio::on_process(void* userdata) noexcept
{
	auto& self = *static_cast<Audio*>(userdata);
	auto& stream = self.stream;

	auto buffer_opt = pw::dequeue_buffer(stream);
	if (!buffer_opt) return;
	auto& [buffer, request] = buffer_opt.value();

	if (self.chart_playback_started) {
		auto const current_time = pw::get_stream_time(stream);
		auto const chart_time = current_time - self.chart_start_time;
		self.chart->get().advance_to(chart_time);
	}
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
	pw::enqueue_buffer(stream, request);
}

}

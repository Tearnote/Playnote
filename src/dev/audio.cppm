/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

dev/audio.cppm:
Initializes audio and submits buffers for playback.
*/

module;
#include "macros/logger.hpp"
#include "macros/assert.hpp"

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

	void play_chart(shared_ptr<bms::Chart> const& chart);

	void pause() { paused = true; }
	void resume() { paused = false; }

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

	bool chart_playback_started = false;
	shared_ptr<bms::Chart> chart;
	atomic<bool> paused;

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

void Audio::play_chart(shared_ptr<bms::Chart> const& chart)
{
	pw::lock_thread_loop(loop);
	this->chart = chart;
	chart_playback_started = true;
	paused = false;
	pw::unlock_thread_loop(loop);
}

void Audio::on_process(void* userdata) noexcept
{
	static constexpr auto Volume = 1.0f / 2.0f;

	auto& self = *static_cast<Audio*>(userdata);
	auto& stream = self.stream;

	auto buffer_opt = pw::dequeue_buffer(stream);
	if (!buffer_opt) return;
	auto& [buffer, request] = buffer_opt.value();

	if (self.chart_playback_started && !self.paused) {
		for (auto& dest: buffer) {
			dest = {};
			ASSUME(dest.left == 0 && dest.right == 0);
			self.chart->advance_one_sample([&](pw::Sample sample) {
				dest.left += sample.left * Volume;
				dest.right += sample.right * Volume;
			});
			if (abs(dest.left) > 1.0f || abs(dest.right) > 1.0f) {
				WARN("Sample overflow detected, clipping");
				dest.left = clamp(dest.left, -1.0f, 1.0f);
				dest.right = clamp(dest.right, -1.0f, 1.0f);
			}
		}
	}

	pw::enqueue_buffer(stream, request);
}

}

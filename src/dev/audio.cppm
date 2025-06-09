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
	static constexpr auto ChannelCount = 2u;
	static constexpr auto Latency = 256u;

	Audio();
	~Audio();

	[[nodiscard]] auto get_sampling_rate() const noexcept -> uint32 { return sampling_rate; }

	void play_chart(shared_ptr<bms::Cursor> const& play, float gain);

	void pause() { paused = true; }
	void resume() { paused = false; }

	Audio(Audio const&) = delete;
	auto operator=(Audio const&) -> Audio& = delete;
	Audio(Audio&&) = delete;
	auto operator=(Audio&&) -> Audio& = delete;

private:
	InstanceLimit<Audio, 1> instance_limit;

	pw::ThreadLoop loop;
	pw::Stream stream;

	atomic<uint32> sampling_rate = 0;

	bool chart_playback_started = false;
	shared_ptr<bms::Cursor> cursor;
	atomic<bool> paused;
	float chart_gain;

	static void on_process(void*) noexcept;
	static void on_param_changed(void*, uint32_t, pw::SPAPod) noexcept;
};

Audio::Audio() {
	pw::init();
	DEBUG("Using libpipewire {}", pw::get_version());

	loop = pw::create_thread_loop();
	stream = pw::create_stream(loop, AppTitle, Latency, &on_process, &on_param_changed, this);
	pw::start_thread_loop(loop);
	while (sampling_rate == 0) yield();
}

Audio::~Audio()
{
	pw::destroy_stream(loop, stream);
	pw::destroy_thread_loop(loop);
}

void Audio::play_chart(shared_ptr<bms::Cursor> const& cursor, float gain)
{
	ASSERT(gain > 0);
	pw::lock_thread_loop(loop);
	this->cursor = cursor;
	chart_playback_started = true;
	paused = false;
	chart_gain = gain;
	pw::unlock_thread_loop(loop);
}

void Audio::on_process(void* userdata) noexcept
{
	auto& self = *static_cast<Audio*>(userdata);
	auto& stream = self.stream;

	auto buffer_opt = pw::dequeue_buffer(stream);
	if (!buffer_opt) return;
	auto& [buffer, request] = buffer_opt.value();

	if (self.chart_playback_started && !self.paused) {
		for (auto& dest: buffer) {
			dest = {};
			ASSUME(dest.left == 0 && dest.right == 0);
			self.cursor->advance_one_sample([&](pw::Sample sample) {
				dest.left += sample.left * self.chart_gain;
				dest.right += sample.right * self.chart_gain;
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

void Audio::on_param_changed(void* userdata, uint32_t id, pw::SPAPod param) noexcept
{
	auto& self = *static_cast<Audio*>(userdata);
	auto new_sampling_rate = pw::get_sampling_rate_from_param(id, param);
	if (!new_sampling_rate) return;
	self.sampling_rate = *new_sampling_rate;
}

}

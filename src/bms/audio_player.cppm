/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/audio_player.cppm:
A cursor wrapper that sends audio samples to the audio device.
*/

module;
#include "macros/assert.hpp"
#include "macros/logger.hpp"
#include "preamble.hpp"

export module playnote.bms.audio_player;

import playnote.logger;
import playnote.dev.window;
import playnote.dev.audio;
import playnote.bms.cursor;
import playnote.bms.chart;

namespace playnote::bms {

export class AudioPlayer: public dev::Generator {
public:
	// Create the audio player with no cursor attached. Will begin to immediately generate blank
	// samples.
	explicit AudioPlayer(dev::GLFW const& glfw, dev::Audio& audio): glfw{glfw}, audio{audio} { audio.add_generator(*this); }
	~AudioPlayer() { audio.remove_generator(*this); }

	// Attach a chart to the player. A new cursor will be created for it.
	void play(Chart const&, bool paused = false);

	// Return the currently playing chart. Requires that a chart is attached.
	[[nodiscard]] auto get_chart() const -> Chart const& { return cursor->get_chart(); }

	// Return a cursor that's at the same position as the sample playing from the speakers
	// right now. This is a best guess estimate based on time elapsed since the last audio buffer.
	[[nodiscard]] auto get_audio_cursor() const -> Cursor;

	// Detach the cursor, stopping all playback.
	void stop() { paused = true; cursor.reset(); }

	// Pause playback. Cursor will not advance.
	void pause() { paused = true; }

	// Resume playback. Cursor will advance again.
	void resume() { paused = false; }

	// Callback for when the audio device starts processing a new buffer. Maintains sync
	// with the CPU timer.
	void begin_buffer();

	// Sample generation callback for the audio device. Advances the cursor by one sample each call,
	// returning the mix of all playing sounds.
	auto next_sample() -> dev::Sample;

	AudioPlayer(AudioPlayer const&) = delete;
	auto operator=(AudioPlayer const&) -> AudioPlayer& = delete;
	AudioPlayer(AudioPlayer&&) = delete;
	auto operator=(AudioPlayer&&) -> AudioPlayer& = delete;

private:
	dev::GLFW const& glfw;
	dev::Audio& audio;
	optional<Cursor> cursor;
	atomic<bool> paused = true;
	float gain;
	nanoseconds timer_slop;
};

void AudioPlayer::play(Chart const& chart, bool paused)
{
	cursor.emplace(chart);
	gain = chart.metrics.gain;
	ASSERT(gain > 0);
	timer_slop = glfw.get_time();
	this->paused = paused;
}

auto AudioPlayer::get_audio_cursor() const -> Cursor
{
	auto const buffer_start_progress =
		cursor->get_progress() > dev::Audio::Latency?
		cursor->get_progress() - dev::Audio::Latency :
		0zu;
	auto const last_buffer_start = timer_slop + dev::Audio::samples_to_ns(buffer_start_progress);
	auto const elapsed = glfw.get_time() - last_buffer_start;
	auto const elapsed_samples = dev::Audio::ns_to_samples(elapsed);
	auto result = Cursor{*cursor};
	result.fast_forward(clamp(elapsed_samples, 0z, static_cast<isize>(dev::Audio::Latency)));
	return result;
}

void AudioPlayer::begin_buffer()
{
	if (paused) return;
	auto const estimated = timer_slop + dev::Audio::samples_to_ns(cursor->get_progress());
	auto const now = glfw.get_time();
	auto const difference = now - estimated;
	timer_slop += difference;
	if (difference > 5ms) WARN("Audio timer was late by {}ms", difference / 1ms);
	if (difference < -5ms) WARN("Audio timer was early by {}ms", -difference / 1ms);
}

auto AudioPlayer::next_sample() -> dev::Sample
{
	if (paused) {
		timer_slop += dev::Audio::samples_to_ns(1);
		return {};
	}
	auto sample_mix = dev::Sample{};
	cursor->advance_one_sample([&](dev::Sample sample) {
		sample_mix.left += sample.left * gain;
		sample_mix.right += sample.right * gain;
	});
	return sample_mix;
}

}

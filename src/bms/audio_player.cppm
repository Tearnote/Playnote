/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/audio_player.cppm:
A cursor wrapper that sends audio samples to the audio device.
*/

module;
#include "macros/assert.hpp"
#include "macros/logger.hpp"

export module playnote.bms.audio_player;

import playnote.preamble;
import playnote.logger;
import playnote.dev.window;
import playnote.dev.audio;
import playnote.bms.cursor;

namespace playnote::bms {

export class AudioPlayer {
public:
	// Create the audio player with no cursor attached. Will begin to immediately generate blank
	// samples.
	explicit AudioPlayer(dev::GLFW const& glfw, dev::Audio& audio): glfw{glfw}, audio{audio} { audio.add_generator(*this); }
	~AudioPlayer() { audio.remove_generator(*this); }

	// Attach a cursor to the player. The cursor will be advanced automatically if unpaused.
	void play(shared_ptr<Cursor> const& cursor, bool paused = false);



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
	shared_ptr<Cursor> cursor;
	atomic<bool> paused = true;
	float gain;
	nanoseconds timer_slop;
};

void AudioPlayer::play(shared_ptr<Cursor> const& cursor, bool paused)
{
	ASSERT(cursor->get_progress() == 0);
	this->cursor = cursor;
	gain = cursor->get_chart().metrics.gain;
	ASSERT(gain > 0);
	timer_slop = glfw.get_time();
	this->paused = paused;
}

void AudioPlayer::begin_buffer()
{
	if (paused) return;
	auto const estimated = timer_slop + dev::Audio::samples_to_ns(cursor->get_progress());
	auto const now = glfw.get_time();
	auto const difference = now - estimated;
	timer_slop += difference;
	if (difference > 1ms) WARN("Audio timer was late by {}ms", difference / 1ms);
	if (difference < -1ms) WARN("Audio timer was early by {}ms", -difference / 1ms);
}

auto AudioPlayer::next_sample() -> dev::Sample
{
	if (paused) return {};
	auto sample_mix = dev::Sample{};
	cursor->advance_one_sample([&](dev::Sample sample) {
		sample_mix.left += sample.left * gain;
		sample_mix.right += sample.right * gain;
	});
	return sample_mix;
}

}

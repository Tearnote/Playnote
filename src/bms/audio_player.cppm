/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/audio_player.cppm:
A cursor wrapper that sends audio samples to the audio device.
*/

module;
#include "macros/assert.hpp"

export module playnote.bms.audio_player;

import playnote.preamble;
import playnote.dev.audio;
import playnote.bms.cursor;

namespace playnote::bms {

export class AudioPlayer {
public:
	// Create the audio player with no cursor attached. Will begin to immediately generate blank
	// samples.
	explicit AudioPlayer(dev::Audio& audio): audio{audio} { audio.add_generator(*this); }
	~AudioPlayer() { audio.remove_generator(*this); }

	// Attach a cursor to the player. The cursor will be advanced automatically if unpaused.
	void play(shared_ptr<Cursor> const& cursor, bool paused = false);

	// Detach the cursor, stopping all playback.
	void stop() { paused = true; cursor.reset(); }

	// Pause playback. Cursor will not advance.
	void pause() { paused = true; }

	// Resume playback. Cursor will advance again.
	void resume() { paused = false; }

	// Sample generation callback for the audio device. Advances the cursor by one sample each call,
	// returning the mix of all playing sounds.
	auto next_sample() -> dev::Sample;

	AudioPlayer(AudioPlayer const&) = delete;
	auto operator=(AudioPlayer const&) -> AudioPlayer& = delete;
	AudioPlayer(AudioPlayer&&) = delete;
	auto operator=(AudioPlayer&&) -> AudioPlayer& = delete;

private:
	dev::Audio& audio;
	shared_ptr<Cursor> cursor;
	atomic<bool> paused = true;
	float gain;
};

void AudioPlayer::play(shared_ptr<Cursor> const& cursor, bool paused)
{
	this->cursor = cursor;
	gain = cursor->get_chart().metrics.gain;
	ASSERT(gain > 0);
	this->paused = paused;
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

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
	explicit AudioPlayer(dev::Audio& audio): audio{audio} { audio.add_generator(*this); }
	~AudioPlayer() { audio.remove_generator(*this); }

	void play(shared_ptr<Cursor> const& cursor);
	void pause() { paused = true; }
	void resume() { paused = false; }

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

void AudioPlayer::play(shared_ptr<Cursor> const& cursor)
{
	this->cursor = cursor;
	gain = cursor->get_chart().metrics.gain;
	ASSERT(gain > 0);
	paused = false;
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

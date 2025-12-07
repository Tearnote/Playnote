/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "dev/audio.hpp"
#include "audio/mixer.hpp"
#include "bms/cursor.hpp"
#include "bms/mapper.hpp"
#include "input.hpp"

namespace playnote::audio {

class Player: public Generator {
public:

	// Create the Player and register it as an audio generator.
	Player();

	~Player() { globals::mixer->remove_generator(*this); }

	// Retrieve the input queue. Input events should be pushed to this queue.
	auto get_input_queue() -> shared_ptr<spsc_queue<UserInput>> { return inbound_inputs; }

	// Register a cursor with the player. From now on, the cursor will be driven by the audio device and user inputs.
	void add_cursor(shared_ptr<bms::Cursor>, bms::Mapper&&);

	// Unregister a cursor.
	void remove_cursor(shared_ptr<bms::Cursor> const&);

	// Return a copy of a cursor advanced to the same position as the sample playing from the speakers right now.
	// This is a best guess estimate based on time elapsed since the last audio buffer.
	[[nodiscard]] auto get_audio_cursor(shared_ptr<bms::Cursor> const&) const -> bms::Cursor;

	void pause() { paused = true; }
	void resume() { paused = false; }

	void begin_buffer();
	auto next_sample() -> dev::Sample;

	Player(Player const&) = delete;
	auto operator=(Player const&) -> Player& = delete;
	Player(Player&&) = delete;
	auto operator=(Player&&) -> Player& = delete;

private:
	struct PlayableCursor {
		shared_ptr<bms::Cursor> cursor;
		bms::Mapper mapper;
		float gain;
		isize_t sample_offset; // Sample count at the time the cursor was started
	};
	struct ActiveSound {
		bms::MD5 md5;
		isize_t channel;
		span<dev::Sample const> audio;
		isize_t position;
		float gain;
	};
	mutex cursors_lock;
	small_vector<PlayableCursor, 4> cursors;
	nanoseconds timer_slop; // Player start time according to the CPU timer. Adjusted over time to maintain sync
	isize_t samples_processed = 0;
	shared_ptr<spsc_queue<UserInput>> inbound_inputs;
	small_vector<UserInput, 16> pending_inputs;
	bool paused = false;
	small_vector<ActiveSound, 128> active_sounds;
};

}

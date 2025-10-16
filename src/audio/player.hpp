/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

audio/player.hpp:
A multiplexer of BMS chart cursors. Attached cursors are driven by the audio device, and input events are forwarded
to them and translated by their associated Input handlers.
*/

#pragma once
#include "preamble.hpp"
#include "dev/window.hpp"
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
		usize sample_offset; // Sample count at the time the cursor was started
	};
	struct ActiveSound {
		lib::openssl::MD5 md5;
		usize channel;
		span<dev::Sample const> audio;
		usize position;
		float gain;
	};
	mutex cursors_lock;
	small_vector<PlayableCursor, 4> cursors;
	nanoseconds timer_slop; // Player start time according to the CPU timer. Adjusted over time to maintain sync
	usize samples_processed = 0;
	shared_ptr<spsc_queue<UserInput>> inbound_inputs;
	small_vector<UserInput, 16> pending_inputs;
	bool paused = false;
	small_vector<ActiveSound, 128> active_sounds;
};

inline Player::Player()
{
	globals::mixer->add_generator(*this);
	timer_slop = globals::glfw->get_time();
	inbound_inputs = make_shared<spsc_queue<UserInput>>();
}

inline void Player::add_cursor(shared_ptr<bms::Cursor> cursor, bms::Mapper&& mapper)
{
	auto lock = lock_guard{cursors_lock};
	auto gain = dev::lufs_to_gain(cursor->get_chart().metadata.loudness);
	cursors.emplace_back(PlayableCursor{
		.cursor = move(cursor),
		.mapper = move(mapper),
		.gain = gain,
		.sample_offset = samples_processed,
	});
}

inline void Player::remove_cursor(shared_ptr<bms::Cursor> const& cursor)
{
	auto lock = lock_guard{cursors_lock};
	auto const it = find(cursors, cursor, &PlayableCursor::cursor);
	if (it != cursors.end()) {
		auto range = remove_if(active_sounds, [&](auto const& item) { return item.md5 == it->cursor->get_chart().md5; });
		active_sounds.erase(range.begin(), range.end());
		cursors.erase(it);
	}
}

inline auto Player::get_audio_cursor(shared_ptr<bms::Cursor> const& cursor) const -> bms::Cursor
{
	auto const it = find(cursors, cursor, &PlayableCursor::cursor);
	if (it != cursors.end()) {
		auto const buffer_start_progress =
			globals::mixer->get_audio().samples_to_ns(samples_processed) > globals::mixer->get_latency()?
			globals::mixer->get_audio().samples_to_ns(samples_processed) - globals::mixer->get_latency() :
			0ns;
		auto const last_buffer_start = timer_slop + buffer_start_progress;
		auto const elapsed = globals::glfw->get_time() - last_buffer_start;
		auto const elapsed_samples = globals::mixer->get_audio().ns_to_samples(elapsed);
		auto result = bms::Cursor{*it->cursor};
		result.fast_forward(clamp(elapsed_samples, 0z, globals::mixer->get_audio().ns_to_samples(globals::mixer->get_latency())));
		return result;
	}
	PANIC();
}

inline void Player::begin_buffer()
{
	// Retrieve new inputs
	auto input = UserInput{};
	while (inbound_inputs->try_dequeue(input)) {
		// Delay to ensure input is in the future
		visit([](auto& i) { i.timestamp += globals::mixer->get_latency(); }, input);
		pending_inputs.emplace_back(input);
	}

	if (paused) return;

	// Adjust timer slop
	auto const estimated = timer_slop + globals::mixer->get_audio().samples_to_ns(samples_processed);
	auto const now = globals::glfw->get_time();
	auto const difference = now - estimated;
	timer_slop += difference;
	if (difference > 5ms) WARN("Audio timer was late by {}ms", difference / 1ms);
	if (difference < -5ms) WARN("Audio timer was early by {}ms", -difference / 1ms);
}

inline auto Player::next_sample() -> dev::Sample
{
	if (paused) {
		timer_slop += globals::mixer->get_audio().samples_to_ns(1);
		return {};
	}

	// Collect inputs happening at this exact sample
	auto const sample_timestamp = timer_slop + globals::mixer->get_audio().samples_to_ns(samples_processed++);
	auto relevant_inputs = small_vector<UserInput, 16>{};
	auto removed = remove_if(pending_inputs, [&](auto const& input) {
		auto input_timestamp = 0ns;
		visit([&](auto const& i) { input_timestamp = i.timestamp; }, input);
		if (input_timestamp <= sample_timestamp) {
			if (sample_timestamp - input_timestamp > 5ms)
				WARN("Input event timestamp more than 5ms in the past");
			relevant_inputs.emplace_back(input);
			return true;
		}
		return false;
	});
	pending_inputs.erase(removed.begin(), removed.end());

	for (auto& cursor: cursors) {
		// Convert inputs with this cursor's mapping
		auto converted_inputs = small_vector<bms::Cursor::LaneInput, 16>{};
		for (auto const& input: relevant_inputs) {
			visit(visitor{
				[&](KeyInput const& i) {
					if (auto const converted = cursor.mapper.from_key(i, cursor.cursor->get_chart().metadata.playstyle)) {
						converted_inputs.emplace_back(bms::Cursor::LaneInput{
							.lane = converted->lane,
							.state = converted->state,
						});
					}
				},
				[&](ButtonInput const& i) {
					if (auto const converted = cursor.mapper.from_button(i, cursor.cursor->get_chart().metadata.playstyle)) {
						converted_inputs.emplace_back(bms::Cursor::LaneInput{
							.lane = converted->lane,
							.state = converted->state,
						});
					}
				},
				[&](AxisInput const& i) {
					auto const converteds = cursor.mapper.submit_axis_input(i, cursor.cursor->get_chart().metadata.playstyle);
					for (auto const& converted: converteds) {
						converted_inputs.emplace_back(bms::Cursor::LaneInput{
							.lane = converted.lane,
							.state = converted.state,
						});
					}
				}
			}, input);
		}
		auto converteds = cursor.mapper.from_axis_state(cursor.cursor->get_chart().metadata.playstyle);
		for (auto const& converted: converteds) {
			converted_inputs.emplace_back(bms::Cursor::LaneInput{
				.lane = converted.lane,
				.state = converted.state,
			});
		}

		// Advance the cursor
		cursor.cursor->advance_one_sample([&](auto ev) {
			auto it = find_if(active_sounds, [&](auto const& s) {
				return s.md5 == cursor.cursor->get_chart().md5 && s.channel == ev.channel;
			});
			if (it == active_sounds.end()) {
				active_sounds.emplace_back(ActiveSound{
					.md5 = cursor.cursor->get_chart().md5,
					.channel = ev.channel,
					.audio = ev.audio,
					.position = 0,
					.gain = cursor.gain,
				});
			} else {
				it->position = 0;
			}
		}, converted_inputs);
	}

	auto sample_mix = dev::Sample{};
	for (auto i = 0zu; i < active_sounds.size();) {
		auto& sound = active_sounds[i];
		sample_mix.left += sound.audio[sound.position].left * sound.gain;
		sample_mix.right += sound.audio[sound.position].right * sound.gain;
		sound.position += 1;
		if (sound.position >= sound.audio.size()) {
			// Swap-and-pop erase
			active_sounds[i] = move(active_sounds.back());
			active_sounds.pop_back();
		} else {
			i += 1;
		}
	}
	return sample_mix;
}

}

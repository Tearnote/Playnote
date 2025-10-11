/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

audio/player.hpp:
A multiplexer of BMS chart cursors. Attached cursors are driven by the audio device, and input events are forwarded
to them and translated by their associated Input handlers.
*/

#pragma once
#include "preamble.hpp"
#include "lib/mpmc.hpp"
#include "dev/window.hpp"
#include "audio/mixer.hpp"
#include "bms/cursor_legacy.hpp"
#include "bms/input.hpp"
#include "threads/input_shouts.hpp"

namespace playnote::audio {

class Player: public Generator {
public:

	// Create the Player and register it as an audio generator.
	Player();

	~Player() { globals::mixer->remove_generator(*this); }

	// Retrieve the input queue. Input events should be pushed to this queue.
	auto get_input_queue() -> shared_ptr<lib::mpmc::Queue<threads::UserInput>> { return inbound_inputs; }

	// Register a cursor with the player. From now on, the cursor will be driven by the audio device and user inputs.
	void add_cursor(shared_ptr<bms::CursorLegacy>, bms::Mapper&&);

	// Unregister a cursor.
	void remove_cursor(shared_ptr<bms::CursorLegacy> const&);

	// Return a copy of a cursor advanced to the same position as the sample playing from the speakers right now.
	// This is a best guess estimate based on time elapsed since the last audio buffer.
	[[nodiscard]] auto get_audio_cursor(shared_ptr<bms::CursorLegacy> const&) const -> bms::CursorLegacy;

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
		shared_ptr<bms::CursorLegacy> cursor;
		bms::Mapper mapper;
		float gain;
		usize sample_offset; // Sample count at the time the cursor was started
	};
	mutex cursors_lock;
	vector<PlayableCursor> cursors;
	nanoseconds timer_slop; // Player start time according to the CPU timer. Adjusted over time to maintain sync
	usize samples_processed = 0;
	shared_ptr<lib::mpmc::Queue<threads::UserInput>> inbound_inputs;
	vector<threads::UserInput> pending_inputs;
	bool paused = false;
};

inline Player::Player()
{
	globals::mixer->add_generator(*this);
	timer_slop = globals::glfw->get_time();
	inbound_inputs = make_shared<lib::mpmc::Queue<threads::UserInput>>();
}

inline void Player::add_cursor(shared_ptr<bms::CursorLegacy> cursor, bms::Mapper&& mapper)
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

inline void Player::remove_cursor(shared_ptr<bms::CursorLegacy> const& cursor)
{
	auto lock = lock_guard{cursors_lock};
	auto const it = find(cursors, cursor, &PlayableCursor::cursor);
	if (it != cursors.end()) cursors.erase(it);
}

inline auto Player::get_audio_cursor(shared_ptr<bms::CursorLegacy> const& cursor) const -> bms::CursorLegacy
{
	auto const it = find(cursors, cursor, &PlayableCursor::cursor);
	if (it != cursors.end()) {
		auto const& playable_cursor = *it;
		auto const buffer_start_progress =
			globals::mixer->get_audio().samples_to_ns(samples_processed) > globals::mixer->get_latency()?
			globals::mixer->get_audio().samples_to_ns(samples_processed) - globals::mixer->get_latency() :
			0ns;
		auto const last_buffer_start = timer_slop + buffer_start_progress;
		auto const elapsed = globals::glfw->get_time() - last_buffer_start;
		auto const elapsed_samples = globals::mixer->get_audio().ns_to_samples(elapsed);
		auto result = bms::CursorLegacy{*it->cursor};
		result.fast_forward(clamp(elapsed_samples, 0z, globals::mixer->get_audio().ns_to_samples(globals::mixer->get_latency())));
		return result;
	}
	PANIC();
}

inline void Player::begin_buffer()
{
	// Retrieve new inputs
	auto input = threads::UserInput{};
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
	auto relevant_inputs = small_vector<threads::UserInput, 16>{};
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

	auto sample_mix = dev::Sample{};
	for (auto& cursor: cursors) {
		// Convert inputs with this cursor's mapping
		auto converted_inputs = small_vector<bms::CursorLegacy::LaneInput, 16>{};
		for (auto const& input: relevant_inputs) {
			visit(visitor{
				[&](threads::KeyInput const& i) {
					if (auto const converted = cursor.mapper.from_key(i, cursor.cursor->get_chart().metadata.playstyle)) {
						converted_inputs.emplace_back(bms::CursorLegacy::LaneInput{
							.lane = converted->lane,
							.state = converted->state,
						});
					}
				},
				[&](threads::ButtonInput const& i) {
					if (auto const converted = cursor.mapper.from_button(i, cursor.cursor->get_chart().metadata.playstyle)) {
						converted_inputs.emplace_back(bms::CursorLegacy::LaneInput{
							.lane = converted->lane,
							.state = converted->state,
						});
					}
				},
				[&](threads::AxisInput const& i) {
					auto const converteds = cursor.mapper.submit_axis_input(i, cursor.cursor->get_chart().metadata.playstyle);
					for (auto const& converted: converteds) {
						converted_inputs.emplace_back(bms::CursorLegacy::LaneInput{
							.lane = converted.lane,
							.state = converted.state,
						});
					}
				}
			}, input);
		}
		auto converteds = cursor.mapper.from_axis_state(cursor.cursor->get_chart().metadata.playstyle);
		for (auto const& converted: converteds) {
			converted_inputs.emplace_back(bms::CursorLegacy::LaneInput{
				.lane = converted.lane,
				.state = converted.state,
			});
		}

		// Advance the cursor
		cursor.cursor->advance_one_sample([&](auto sample) {
			sample_mix.left += sample.left * cursor.gain;
			sample_mix.right += sample.right * cursor.gain;
		}, converted_inputs);
	}

	return sample_mix;
}

}

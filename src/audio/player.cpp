/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "audio/player.hpp"

#include "preamble.hpp"
#include "utils/assert.hpp"
#include "utils/logger.hpp"
#include "dev/window.hpp"

namespace playnote::audio {

Player::Player()
{
	globals::mixer->add_generator(*this);
	timer_slop = globals::glfw->get_time();
	inbound_inputs = make_shared<spsc_queue<UserInput>>();
}

void Player::add_cursor(shared_ptr<bms::Cursor> cursor, bms::Mapper&& mapper)
{
	ASSERT(cursor->get_chart().media.sampling_rate == globals::mixer->get_audio().get_sampling_rate());
	auto lock = lock_guard{cursors_lock};
	auto gain = dev::lufs_to_gain(cursor->get_chart().metadata.loudness);
	cursors.emplace_back(PlayableCursor{
		.cursor = move(cursor),
		.mapper = move(mapper),
		.gain = gain,
		.sample_offset = samples_processed,
	});
}

void Player::remove_cursor(shared_ptr<bms::Cursor> const& cursor)
{
	auto lock = lock_guard{cursors_lock};
	auto const it = find(cursors, cursor, &PlayableCursor::cursor);
	if (it != cursors.end()) {
		auto range = remove_if(active_sounds, [&](auto const& item) { return item.md5 == it->cursor->get_chart().md5; });
		active_sounds.erase(range.begin(), range.end());
		cursors.erase(it);
	}
}

auto Player::get_audio_cursor(shared_ptr<bms::Cursor> const& cursor) const -> bms::Cursor
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
		result.seek_relative(clamp(elapsed_samples, 0z, globals::mixer->get_audio().ns_to_samples(globals::mixer->get_latency())));
		return result;
	}
	PANIC();
}

void Player::begin_buffer()
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

auto Player::next_sample() -> dev::Sample
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
	for (auto i = 0z; i < static_cast<ssize_t>(active_sounds.size());) {
		auto& sound = active_sounds[i];
		sample_mix.left += sound.audio[sound.position].left * sound.gain;
		sample_mix.right += sound.audio[sound.position].right * sound.gain;
		sound.position += 1;
		if (sound.position >= static_cast<ssize_t>(sound.audio.size())) {
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

/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

audio/player.hpp:
A cursor wrapper that sends audio samples to the audio device.
*/

#pragma once
#include "preamble.hpp"
#include "assert.hpp"
#include "logger.hpp"
#include "dev/window.hpp"
#include "audio/mixer.hpp"
#include "bms/cursor_legacy.hpp"
#include "bms/chart.hpp"
#include "bms/input.hpp"

namespace playnote::audio {

class PlayerLegacy: public Generator {
public:
	// Create the audio player with no cursor attached. Will begin to immediately generate blank
	// samples.
	PlayerLegacy() { globals::mixer->add_generator(*this); }
	~PlayerLegacy() { globals::mixer->remove_generator(*this); }

	// Attach a chart to the player. A new cursor will be created for it.
	void play(bms::Chart const&, bool autoplay, bool paused = false);

	// Return true if a chart is currently attached.
	[[nodiscard]] auto is_playing() const -> bool { return cursor.has_value(); }

	// Register a player input to be processed when its timestamp arrives.
	void enqueue_input(bms::Input input);

	// Return the currently playing chart. Requires that a chart is attached.
	[[nodiscard]] auto get_chart() const -> optional<reference_wrapper<bms::Chart const>>;

	// Return a cursor that's at the same position as the sample playing from the speakers
	// right now. This is a best guess estimate based on time elapsed since the last audio buffer.
	[[nodiscard]] auto get_audio_cursor() const -> optional<bms::CursorLegacy>;

	// Convert an absolute timestamp to one relative to the chart's start time.
	[[nodiscard]] auto chart_relative_timestamp(nanoseconds) const -> nanoseconds;

	// Detach the cursor, stopping all playback.
	void stop();

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

	PlayerLegacy(PlayerLegacy const&) = delete;
	auto operator=(PlayerLegacy const&) -> PlayerLegacy& = delete;
	PlayerLegacy(PlayerLegacy&&) = delete;
	auto operator=(PlayerLegacy&&) -> PlayerLegacy& = delete;

private:
	struct PendingCommand {
		enum class Type {
			Start,
			Stop,
		};
		Type type;
		optional<bms::CursorLegacy> new_cursor;
		optional<bool> new_paused;
	};
	optional<bms::CursorLegacy> cursor;
	optional<PendingCommand> pending_command;
	atomic<bool> paused = true;
	float gain;
	nanoseconds timer_slop; // Chart start time according to the CPU timer. Adjusted over time to maintain sync
	vector<bms::Input> inputs;
};

inline void PlayerLegacy::play(bms::Chart const& chart, bool autoplay, bool paused)
{
	pending_command = PendingCommand{
		.type = PendingCommand::Type::Start,
		.new_cursor = bms::CursorLegacy{chart, autoplay},
		.new_paused = paused,
	};
}

inline void PlayerLegacy::enqueue_input(bms::Input input)
{
	if (!cursor) return;
	input.timestamp = chart_relative_timestamp(input.timestamp) + globals::mixer->get_latency();
	inputs.emplace_back(input);
}

inline auto PlayerLegacy::get_chart() const -> optional<reference_wrapper<bms::Chart const>>
{
	if (!cursor) return nullopt;
	return cursor->get_chart();
}

inline auto PlayerLegacy::get_audio_cursor() const -> optional<bms::CursorLegacy>
{
	if (!cursor) return nullopt;
	auto const buffer_start_progress =
		cursor->get_progress_ns() > globals::mixer->get_latency()?
		cursor->get_progress_ns() - globals::mixer->get_latency() :
		0ns;
	auto const last_buffer_start = timer_slop + buffer_start_progress;
	auto const elapsed = globals::glfw->get_time() - last_buffer_start;
	auto const elapsed_samples = globals::mixer->get_audio().ns_to_samples(elapsed);
	auto result = bms::CursorLegacy{*cursor};
	result.fast_forward(clamp(elapsed_samples, 0z, globals::mixer->get_audio().ns_to_samples(globals::mixer->get_latency())));
	return result;
}

inline auto PlayerLegacy::chart_relative_timestamp(nanoseconds time) const -> nanoseconds
{
	return time - timer_slop;
}

inline void PlayerLegacy::stop()
{
	pending_command = PendingCommand{ .type = PendingCommand::Type::Stop };
}

inline void PlayerLegacy::begin_buffer()
{
	if (pending_command) {
		switch (pending_command->type) {
		case PendingCommand::Type::Start:
			cursor = pending_command->new_cursor;
			paused = *pending_command->new_paused;
			gain = dev::lufs_to_gain(cursor->get_chart().metadata.loudness);
			ASSERT(gain > 0);
			timer_slop = globals::glfw->get_time();
			break;
		case PendingCommand::Type::Stop:
			cursor = nullopt;
			paused = true;
			break;
		}
		pending_command = nullopt;
	}

	if (paused) return;
	auto const estimated = timer_slop + globals::mixer->get_audio().samples_to_ns(cursor->get_progress());
	auto const now = globals::glfw->get_time();
	auto const difference = now - estimated;
	timer_slop += difference;
	if (difference > 5ms) WARN("Audio timer was late by {}ms", difference / 1ms);
	if (difference < -5ms) WARN("Audio timer was early by {}ms", -difference / 1ms);
}

inline auto PlayerLegacy::next_sample() -> dev::Sample
{
	static auto new_inputs = vector<bms::CursorLegacy::LaneInput>{};
	if (paused) {
		timer_slop += globals::mixer->get_audio().samples_to_ns(1);
		return {};
	}

	new_inputs.clear();
	auto removed = remove_if(inputs, [&](auto const& input) {
		if (input.timestamp <= cursor->get_progress_ns()) {
			if (cursor->get_progress_ns() - input.timestamp > 5ms)
				WARN("Input event timestamp more than 5ms in the past");
			new_inputs.emplace_back(bms::CursorLegacy::LaneInput{
				.lane = input.lane,
				.state = input.state,
			});
			return true;
		}
		return false;
	});
	inputs.erase(removed.begin(), removed.end());

	auto sample_mix = dev::Sample{};
	cursor->advance_one_sample([&](dev::Sample sample) {
		sample_mix.left += sample.left * gain;
		sample_mix.right += sample.right * gain;
	}, new_inputs);
	return sample_mix;
}

}

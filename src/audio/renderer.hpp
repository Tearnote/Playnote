/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

audio/renderer.hpp:
An offline chart audio renderer.
*/

#pragma once
#include "preamble.hpp"
#include "dev/audio.hpp"
#include "bms/cursor.hpp"
#include "bms/chart.hpp"

namespace playnote::audio {

class Renderer {
public:
	// Create a renderer for the given chart.
	explicit Renderer(shared_ptr<bms::Chart const>);

	[[nodiscard]] auto get_cursor() const -> bms::Cursor const& { return *cursor; }

	// Advance chart playback by one audio sample. If nullopt is returned, the chart has ended.
	auto advance_one_sample() -> optional<dev::Sample>;

private:
	struct ActiveSound {
		usize channel;
		span<dev::Sample const> audio;
		usize position;
	};
	shared_ptr<bms::Chart const> chart;
	shared_ptr<bms::Cursor> cursor;
	small_vector<ActiveSound, 128> active_sounds;
};

inline Renderer::Renderer(shared_ptr<bms::Chart const> chart):
	chart{move(chart)},
	cursor{make_shared<bms::Cursor>(this->chart, true)}
{}

inline auto Renderer::advance_one_sample() -> optional<dev::Sample>
{
	auto const chart_ended = !cursor->advance_one_sample([&](auto ev) {
		auto it = find_if(active_sounds, [&](auto const& s) {
			return s.channel == ev.channel;
		});
		if (it == active_sounds.end()) {
			active_sounds.emplace_back(ActiveSound{
				.channel = ev.channel,
				.audio = ev.audio,
				.position = 0,
			});
		} else {
			it->position = 0;
		}
	});
	if (chart_ended && active_sounds.empty()) return nullopt;
	auto sample_mix = dev::Sample{};
	for (auto i = 0zu; i < active_sounds.size();) {
		auto& sound = active_sounds[i];
		sample_mix.left += sound.audio[sound.position].left;
		sample_mix.right += sound.audio[sound.position].right;
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

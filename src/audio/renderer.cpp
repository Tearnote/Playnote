/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "audio/renderer.hpp"

#include "preamble.hpp"

namespace playnote::audio {

Renderer::Renderer(shared_ptr<bms::Chart const> chart):
	chart{move(chart)},
	cursor{make_shared<bms::Cursor>(this->chart, true)}
{}

void Renderer::seek(nanoseconds time)
{
	cursor->seek_ns(time);
	active_sounds.clear();
}

auto Renderer::advance_one_sample() -> optional<dev::Sample>
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
	for (auto i = 0z; i < static_cast<ssize_t>(active_sounds.size());) {
		auto& sound = active_sounds[i];
		sample_mix.left += sound.audio[sound.position].left;
		sample_mix.right += sound.audio[sound.position].right;
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

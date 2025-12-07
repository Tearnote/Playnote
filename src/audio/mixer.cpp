/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "audio/mixer.hpp"

#include "preamble.hpp"

namespace playnote::audio {

Mixer::Mixer(Logger::Category cat):
	cat{cat},
	// This might call mix() in another thread before initialization is finished, but we quit early
	// if no generators were added yet
	audio{cat, [this](span<dev::Sample> buffer) { this->mix(buffer); }},
	limiter{audio.get_sampling_rate(), 1ms, 10ms, 100ms}
{}

void Mixer::mix(span<dev::Sample> buffer)
{
	// This should only block during startup/shutdown and loadings
	auto lock = lock_guard{generator_lock};
	if (generators.empty()) return;

	for (auto const& generator: generators)
		generator.second.begin_buffer();
	for (auto& dest: buffer) {
		auto next = dev::Sample{};
		for (auto const& generator: generators) {
			auto const sample = generator.second.next_sample();
			next.left += sample.left;
			next.right += sample.right;
		}
		dest = limiter.process(next);
	}
}

}

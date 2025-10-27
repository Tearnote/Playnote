/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once

#include "preamble.hpp"
#include "utils/service.hpp"
#include "utils/logger.hpp"
#include "lib/signalsmith.hpp"
#include "dev/audio.hpp"

namespace playnote::audio {

// A trait for a type that can serve as an audio generator.
class Generator {
public:
	auto next_sample() -> dev::Sample = delete;
	void begin_buffer() = delete;
};

class Mixer {
public:
	explicit Mixer(Logger::Category);

	// Register an audio generator. A generator is any object that implements the member function
	// auto next_sample() -> dev::Sample.
	template<implements<Generator> T>
	void add_generator(T& generator);

	// Unregister an audio generator. Make sure to unregister a generator before destroying it.
	template<implements<Generator> T>
	void remove_generator(T& generator);

	// Return the dev::Audio instance internally managed by the mixer.
	[[nodiscard]] auto get_audio() -> dev::Audio& { return audio; }

	// Return current latency of the mixer, which includes both the audio device latency
	// and the latency of any active effects.
	[[nodiscard]] auto get_latency() -> nanoseconds { return audio.get_latency() + 1ms; }

private:
	Logger::Category cat;
	dev::Audio audio;

	struct GeneratorOps {
		function<void()> begin_buffer;
		function<dev::Sample()> next_sample;
	};
	unordered_map<void*, GeneratorOps> generators;
	mutex generator_lock;

	lib::dsp::Limiter limiter;

	void mix(span<dev::Sample>);
};

inline Mixer::Mixer(Logger::Category cat):
	cat{cat},
	// This might call mix() in another thread before initialization is finished, but we quit early
	// if no generators were added yet
	audio{cat, [this](span<dev::Sample> buffer) { this->mix(buffer); }},
	limiter{audio.get_sampling_rate(), 1ms, 10ms, 100ms}
{}

template<implements<Generator> T>
void Mixer::add_generator(T& generator) {
	auto lock = lock_guard{generator_lock};
	generators.emplace(&generator, GeneratorOps{
		[&]() { generator.begin_buffer(); },
		[&]() { return generator.next_sample(); },
	});
	TRACE_AS(cat, "Added generator to the mixer");
}

template<implements<Generator> T>
void Mixer::remove_generator(T& generator)
{
	auto lock = lock_guard{generator_lock};
	generators.erase(&generator);
	TRACE_AS(cat, "Removed generator from the mixer");
}

inline void Mixer::mix(span<dev::Sample> buffer)
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

namespace playnote::globals {
inline auto mixer = Service<audio::Mixer>{};
}

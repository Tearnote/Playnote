/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

audio/mixer.hpp:
Wrapper over an Audio instance that provides playback of BMS charts and other audio.
*/

#pragma once

#include "preamble.hpp"
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
	Mixer();

	// Register an audio generator. A generator is any object that implements the member function
	// auto next_sample() -> dev::Sample.
	template<implements<Generator> T>
	void add_generator(T& generator);

	// Unregister an audio generator. Make sure to unregister a generator before destroying it.
	template<implements<Generator> T>
	void remove_generator(T& generator);

	// Return current latency of the mixer, which includes both the audio device latency
	// and the latency of any active effects.
	[[nodiscard]] static auto get_latency() -> nanoseconds { return dev::Audio::get_latency() + 1ms; }

private:
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

inline Mixer::Mixer():
	// This might call mix() in another thread before initialization is finished, but we quit early
	// if no generators were added yet
	audio{[this](span<dev::Sample> buffer) { this->mix(buffer); }},
	limiter{audio.get_sampling_rate(), 1ms, 10ms, 100ms}
{}

template<implements<Generator> T>
void Mixer::add_generator(T& generator) {
	auto lock = lock_guard{generator_lock};
	generators.emplace(&generator, GeneratorOps{
		[&]() { generator.begin_buffer(); },
		[&]() { return generator.next_sample(); },
	});
}

template<implements<Generator> T>
void Mixer::remove_generator(T& generator)
{
	auto lock = lock_guard{generator_lock};
	generators.erase(&generator);
}

inline void Mixer::mix(span<dev::Sample> buffer)
{
	// Mutexes are bad in realtime context, but this should only block during startup/shutdown
	// and loadings.
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

/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

dev/audio.cppm:
Initializes audio and submits buffers for playback, filled with audio from registered generators.
*/

module;
#include "macros/assert.hpp"
#include "preamble.hpp"
#include "config.hpp"
#include "logger.hpp"

export module playnote.dev.audio;

import playnote.lib.pipewire;

namespace playnote::dev {

namespace pw = lib::pw;

// A single audio sample.
export using Sample = pw::Sample;

// A trait for a type that can serve as an audio generator.
export class Generator {
public:
	auto next_sample() -> Sample = delete;
	void begin_buffer() = delete;
};

export class Audio {
public:
	static constexpr auto ChannelCount = 2zu;
	static constexpr auto Latency = 128zu;

	Audio();
	~Audio();

	// Return current sampling rate. The value is only valid while an Audio instance exists.
	[[nodiscard]] static auto get_sampling_rate() -> uint32 { return ASSERT_VAL(sampling_rate); }

	// Convert a count of samples to their duration.
	[[nodiscard]] static auto samples_to_ns(isize) -> nanoseconds;

	// Convert a duration to a number of full audio samples.
	[[nodiscard]] static auto ns_to_samples(nanoseconds) -> isize;

	// Register an audio generator. A generator is any object that implements the member function
	// auto next_sample() -> Sample.
	template<implements<Generator> T>
	void add_generator(T& generator);

	// Unregister an audio generator. Make sure to unregister a generator before destroying it.
	template<implements<Generator> T>
	void remove_generator(T& generator);

	Audio(Audio const&) = delete;
	auto operator=(Audio const&) -> Audio& = delete;
	Audio(Audio&&) = delete;
	auto operator=(Audio&&) -> Audio& = delete;

private:
	struct GeneratorOps {
		function<void()> begin_buffer;
		function<Sample()> next_sample;
	};

	InstanceLimit<Audio, 1> instance_limit;

	static inline atomic<uint32> sampling_rate = 0;

	pw::ThreadLoop loop;
	pw::Stream stream;

	unordered_map<void*, GeneratorOps> generators;
	mutex generator_lock;

	static void on_process(void*);
	static void on_param_changed(void*, uint32_t, pw::SPAPod);
};

template<implements<Generator> T>
void Audio::add_generator(T& generator) {
	auto lock = lock_guard{generator_lock};
	generators.emplace(&generator, GeneratorOps{
		[&]() { generator.begin_buffer(); },
		[&]() { return generator.next_sample(); },
	});
}

template<implements<Generator> T>
void Audio::remove_generator(T& generator)
{
	auto lock = lock_guard{generator_lock};
	generators.erase(&generator);
}

Audio::Audio() {
	pw::init();
	DEBUG("Using libpipewire {}", pw::get_version());

	loop = pw::create_thread_loop();
	stream = pw::create_stream(loop, AppTitle, Latency, &on_process, &on_param_changed, this);
	pw::start_thread_loop(loop);
	while (sampling_rate == 0) yield();
}

Audio::~Audio()
{
	pw::destroy_stream(loop, stream);
	pw::destroy_thread_loop(loop);
}

void Audio::on_process(void* userdata)
{
	auto& self = *static_cast<Audio*>(userdata);
	auto& stream = self.stream;
	// Mutexes are bad in realtime context, but this should only block during startup/shutdown
	// and loadings.
	auto lock = lock_guard{self.generator_lock};

	auto buffer_opt = pw::dequeue_buffer(stream);
	if (!buffer_opt) return;
	auto& [buffer, request] = buffer_opt.value();

	for (auto const& generator: self.generators | views::values)
		generator.begin_buffer();

	for (auto& dest: buffer) {
		dest = {};
		for (auto const& generator: self.generators | views::values) {
			auto const sample = generator.next_sample();
			dest.left += sample.left;
			dest.right += sample.right;
		}
		if (abs(dest.left) > 1.0f || abs(dest.right) > 1.0f) {
			WARN("Sample overflow detected, clipping");
			dest.left = clamp(dest.left, -1.0f, 1.0f);
			dest.right = clamp(dest.right, -1.0f, 1.0f);
		}
	}

	pw::enqueue_buffer(stream, request);
}

void Audio::on_param_changed(void*, uint32_t id, pw::SPAPod param)
{
	auto const new_sampling_rate = pw::get_sampling_rate_from_param(id, param);
	if (!new_sampling_rate) return;
	sampling_rate = *new_sampling_rate;
}

auto Audio::samples_to_ns(isize samples) -> nanoseconds
{
	ASSERT(sampling_rate > 0);
	auto const ns_per_sample = duration_cast<nanoseconds>(duration<double>{1.0 / sampling_rate});
	auto const whole_seconds = samples / sampling_rate;
	auto const remainder = samples % sampling_rate;
	return 1s * whole_seconds + ns_per_sample * remainder;
}

auto Audio::ns_to_samples(nanoseconds ns) -> isize
{
	ASSERT(sampling_rate > 0);
	auto const ns_per_sample = duration_cast<nanoseconds>(duration<double>{1.0 / sampling_rate});
	return ns / ns_per_sample;
}

}

/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

dev/audio.hpp:
Initializes audio and submits buffers for playback, filled with audio from registered generators.
*/

#pragma once
#include "preamble.hpp"
#include "assert.hpp"
#include "config.hpp"
#include "logger.hpp"
#include "lib/signalsmith.hpp"
#include "lib/pipewire.hpp"
#ifdef _WIN32
#include "lib/wasapi.hpp"
#endif

namespace playnote::dev {

// A single audio sample.
using Sample = lib::pw::Sample;

// A trait for a type that can serve as an audio generator.
class Generator {
public:
	auto next_sample() -> Sample = delete;
	void begin_buffer() = delete;
};

class Audio {
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

#ifndef _WIN32
	lib::pw::ThreadLoop loop;
	lib::pw::Stream stream;
#else
	lib::wasapi::Context context;
#endif

	unordered_map<void*, GeneratorOps> generators;
	mutex generator_lock;

	optional<lib::dsp::Limiter> limiter;

	static void on_process(void*);
#ifndef _WIN32
	static void on_param_changed(void*, uint32_t, lib::pw::SPAPod);
#endif
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

inline Audio::Audio() {
#ifndef _WIN32
	lib::pw::init();
	DEBUG("Using libpipewire {}", lib::pw::get_version());

	loop = lib::pw::create_thread_loop();
	stream = lib::pw::create_stream(loop, AppTitle, Latency, &on_process, &on_param_changed, this);
	lib::pw::start_thread_loop(loop);
	while (sampling_rate.load() == 0) yield();
#else
	context = lib::wasapi::init(true, &on_process, this);
	sampling_rate.store(context.sampling_rate);
#endif
	limiter.emplace(sampling_rate, 1ms, 10ms, 100ms);
	DEBUG("Audio device sample rate: {} Hz", sampling_rate.load());
}

inline Audio::~Audio()
{
#ifndef _WIN32
	lib::pw::destroy_stream(loop, stream);
	lib::pw::destroy_thread_loop(loop);
#else
	lib::wasapi::cleanup(move(context));
#endif
}

inline void Audio::on_process(void* userdata)
{
	auto& self = *static_cast<Audio*>(userdata);
	// Mutexes are bad in realtime context, but this should only block during startup/shutdown
	// and loadings.
	auto lock = lock_guard{self.generator_lock};

#ifndef _WIN32
	auto& stream = self.stream;
	auto buffer_opt = lib::pw::dequeue_buffer(stream);
	if (!buffer_opt) return;
	auto& [buffer, request] = buffer_opt.value();
#else
	auto buffer = lib::wasapi::dequeue_buffer(self.context);
#endif

	for (auto const& generator: self.generators)
		generator.second.begin_buffer();
	for (auto& dest: buffer) {
		auto next = lib::pw::Sample{};
		for (auto const& generator: self.generators) {
			auto const sample = generator.second.next_sample();
			next.left += sample.left;
			next.right += sample.right;
		}

		if (!self.limiter) continue;
		dest = self.limiter->process(next);
	}
#ifndef _WIN32
	lib::pw::enqueue_buffer(stream, request);
#else
	lib::wasapi::enqueue_buffer(self.context);
#endif
}

#ifndef _WIN32
inline void Audio::on_param_changed(void*, uint32_t id, lib::pw::SPAPod param)
{
	auto const new_sampling_rate = lib::pw::get_sampling_rate_from_param(id, param);
	if (!new_sampling_rate) return;
	sampling_rate.store(*new_sampling_rate);
}
#endif

inline auto Audio::samples_to_ns(isize samples) -> nanoseconds
{
	auto const rate = sampling_rate.load();
	ASSERT(rate > 0);
	auto const ns_per_sample = duration_cast<nanoseconds>(duration<double>{1.0 / rate});
	auto const whole_seconds = samples / rate;
	auto const remainder = samples % rate;
	return 1s * whole_seconds + ns_per_sample * remainder;
}

inline auto Audio::ns_to_samples(nanoseconds ns) -> isize
{
	auto const rate = sampling_rate.load();
	ASSERT(rate > 0);
	auto const ns_per_sample = duration_cast<nanoseconds>(duration<double>{1.0 / rate});
	return ns / ns_per_sample;
}

}

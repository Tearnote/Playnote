/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

dev/audio.hpp:
Abstraction over the available audio backends.
*/

#pragma once
#include "preamble.hpp"
#include "assert.hpp"
#include "config.hpp"
#include "logger.hpp"
#include "lib/audio_common.hpp"
#ifdef _WIN32
#include "lib/wasapi.hpp"
#else
#include "lib/pipewire.hpp"
#endif

namespace playnote::dev {

// A single audio sample.
using Sample = lib::Sample;

using lib::ChannelCount;

class Audio {
public:
	static constexpr auto Latency = 128zu;

	template<callable<void(span<Sample>)> Func>
	explicit Audio(Func&& generator);
	~Audio();

	// Return current sampling rate. The value is only valid while an Audio instance exists.
	[[nodiscard]] static auto get_sampling_rate() -> uint32 { return ASSERT_VAL(context->properties.sampling_rate); }

	// Convert a count of samples to their duration.
	[[nodiscard]] static auto samples_to_ns(isize) -> nanoseconds;

	// Convert a duration to a number of full audio samples.
	[[nodiscard]] static auto ns_to_samples(nanoseconds) -> isize;

	Audio(Audio const&) = delete;
	auto operator=(Audio const&) -> Audio& = delete;
	Audio(Audio&&) = delete;
	auto operator=(Audio&&) -> Audio& = delete;

private:
	InstanceLimit<Audio, 1> instance_limit;

#ifndef _WIN32
	static inline lib::pw::Context context;
#else
	static inline lib::wasapi::Context context;
#endif

	function<void(span<Sample>)> generator;

	void on_process(span<Sample> buffer) const { generator(buffer); }
};

template<callable<void(span<Sample>)> Func>
Audio::Audio(Func&& generator):
	generator{generator}
{
#ifndef _WIN32
	context = lib::pw::init(AppTitle, Latency, [this](auto buffer) { on_process(buffer); });
#else
	context = lib::wasapi::init(true, [this](auto buffer) { on_process(buffer); });
#endif
	DEBUG("Audio device sample rate: {} Hz", context->properties.sampling_rate);
}

inline Audio::~Audio()
{
#ifndef _WIN32
	lib::pw::cleanup(move(context));
#else
	lib::wasapi::cleanup(move(context));
#endif
}

inline auto Audio::samples_to_ns(isize samples) -> nanoseconds
{
	auto const rate = context->properties.sampling_rate;
	ASSERT(rate > 0);
	auto const ns_per_sample = duration_cast<nanoseconds>(duration<double>{1.0 / rate});
	auto const whole_seconds = samples / rate;
	auto const remainder = samples % rate;
	return 1s * whole_seconds + ns_per_sample * remainder;
}

inline auto Audio::ns_to_samples(nanoseconds ns) -> isize
{
	auto const rate = context->properties.sampling_rate;
	ASSERT(rate > 0);
	auto const ns_per_sample = duration_cast<nanoseconds>(duration<double>{1.0 / rate});
	return ns / ns_per_sample;
}

}

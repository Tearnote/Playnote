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
#ifdef TARGET_WINDOWS
#include "lib/wasapi.hpp"
#elifdef TARGET_LINUX
#include "lib/pipewire.hpp"
#endif

namespace playnote::dev {

// A single audio sample.
using Sample = lib::Sample;

using lib::ChannelCount;

class Audio {
public:
	template<callable<void(span<Sample>)> Func>
	explicit Audio(Func&& generator);
	~Audio();

	// Return current sampling rate. The value is only valid while an Audio instance exists.
	[[nodiscard]] static auto get_sampling_rate() -> uint32 { return ASSERT_VAL(context->properties.sampling_rate); }

	// Return current latency of the audio device.
	[[nodiscard]] static auto get_latency() -> nanoseconds { return samples_to_ns(context->properties.buffer_size); }

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

#ifdef TARGET_LINUX
	static inline lib::pw::Context context;
#elifdef TARGET_WINDOWS
	static inline lib::wasapi::Context context;
#endif

	function<void(span<Sample>)> generator;

	void on_process(span<Sample> buffer) const { generator(buffer); }
};

template<callable<void(span<Sample>)> Func>
Audio::Audio(Func&& generator):
	generator{generator}
{
#ifdef TARGET_LINUX
	context = lib::pw::init(AppTitle, globals::config->get_entry<int>("pipewire", "buffer_size"),
		[this](auto buffer) { on_process(buffer); });
#elifdef TARGET_WINDOWS
	context = lib::wasapi::init(globals::config->get_entry<bool>("wasapi", "exclusive_mode"),
		[this](auto buffer) { on_process(buffer); },
		globals::config->get_entry<bool>("wasapi", "use_custom_latency")?
			make_optional(milliseconds{globals::config->get_entry<int>("wasapi", "custom_latency")}) : nullopt);
#endif
	INFO("Audio device properties: sample rate: {}Hz, latency: {}ms",
		context->properties.sampling_rate,
		duration_cast<milliseconds>(lib::audio_latency(context->properties)).count());
}

inline Audio::~Audio()
{
#ifdef TARGET_LINUX
	lib::pw::cleanup(move(context));
#elifdef TARGET_WINDOWS
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

// Converts LUFS relative to target to an amplitude gain value.
[[nodiscard]] inline auto lufs_to_gain(double lufs) -> float
{
	constexpr auto LufsTarget = -14.0;
	auto const db_from_target = LufsTarget - lufs;
	auto const amplitude_ratio = pow(10.0, db_from_target / 20.0);
	return static_cast<float>(amplitude_ratio);
}

}

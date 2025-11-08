/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "utils/assert.hpp"
#include "utils/config.hpp"
#include "utils/logger.hpp"
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
	// Initialize the audio device. The provider generator function is called repeatedly to fill in the sample buffer.
	template<callable<void(span<Sample>)> Func>
	explicit Audio(Logger::Category, Func&& generator);
	~Audio();

	// Return current sampling rate. The value is only valid while an Audio instance exists.
	[[nodiscard]] auto get_sampling_rate() -> int { return ASSERT_VAL(context->properties.sampling_rate); }

	// Return current latency of the audio device.
	[[nodiscard]] auto get_latency() -> nanoseconds { return samples_to_ns(context->properties.buffer_size); }

	// Convert a count of samples to their duration.
	[[nodiscard]] auto samples_to_ns(isize_t, int sampling_rate = -1) -> nanoseconds;

	// Convert a duration to a number of full audio samples.
	[[nodiscard]] auto ns_to_samples(nanoseconds, int sampling_rate = -1) -> isize_t;

	Audio(Audio const&) = delete;
	auto operator=(Audio const&) -> Audio& = delete;
	Audio(Audio&&) = delete;
	auto operator=(Audio&&) -> Audio& = delete;

private:
	InstanceLimit<Audio, 1> instance_limit;

	Logger::Category cat;

#ifdef TARGET_LINUX
	lib::pw::Context context;
#elifdef TARGET_WINDOWS
	lib::wasapi::Context context;
#endif

	function<void(span<Sample>)> generator;

	void on_process(span<Sample> buffer) const { generator(buffer); }
};

template<callable<void(span<Sample>)> Func>
Audio::Audio(Logger::Category cat, Func&& generator):
	cat{cat},
	generator{generator}
{
#ifdef TARGET_LINUX
	context = lib::pw::init(AppTitle, globals::config->get_entry<int>("pipewire", "buffer_size"),
		[this](auto buffer) { on_process(buffer); });
	INFO_AS(cat, "Pipewire audio initialized");
#elifdef TARGET_WINDOWS
	context = lib::wasapi::init(cat, globals::config->get_entry<bool>("wasapi", "exclusive_mode"),
		[this](auto buffer) { on_process(buffer); },
		globals::config->get_entry<bool>("wasapi", "use_custom_latency")?
			make_optional(milliseconds{globals::config->get_entry<int>("wasapi", "custom_latency")}) : nullopt);
	INFO_AS(cat, "WASAPI {} mode audio initialized", context->exclusive_mode? "exclusive" : "shared");
#endif
	INFO_AS(cat, "Audio device properties: sample rate: {}Hz, latency: {}ms",
		context->properties.sampling_rate,
		duration_cast<milliseconds>(lib::audio_latency(context->properties)).count());
}

inline Audio::~Audio()
{
#ifdef TARGET_LINUX
	lib::pw::cleanup(move(context));
	INFO_AS(cat, "Pipewire audio cleaned up");
#elifdef TARGET_WINDOWS
	lib::wasapi::cleanup(move(context));
	INFO_AS(cat, "WASAPI audio cleaned up");
#endif
}

inline auto Audio::samples_to_ns(isize_t samples, int sampling_rate) -> nanoseconds
{
	auto const rate = sampling_rate == -1? context->properties.sampling_rate : sampling_rate;
	ASSERT(rate > 0);
	auto const ns_per_sample = duration_cast<nanoseconds>(duration<double>{1.0 / rate});
	auto const whole_seconds = samples / rate;
	auto const remainder = samples % rate;
	return 1s * whole_seconds + ns_per_sample * remainder;
}

inline auto Audio::ns_to_samples(nanoseconds ns, int sampling_rate) -> isize_t
{
	auto const rate = sampling_rate == -1? context->properties.sampling_rate : sampling_rate;
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

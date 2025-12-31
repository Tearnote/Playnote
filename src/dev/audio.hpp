/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

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
	Audio(Logger::Category, function<void(span<Sample>)> generator);
	~Audio() noexcept;

	// Return current sampling rate. The value is only valid while an Audio instance exists.
	[[nodiscard]] auto get_sampling_rate() -> int { return ASSERT_VAL(context->properties.sampling_rate); }

	// Return current latency of the audio device.
	[[nodiscard]] auto get_latency() -> nanoseconds { return samples_to_ns(context->properties.buffer_size); }

	// Convert a count of samples to their duration.
	[[nodiscard]] auto samples_to_ns(ssize_t, int sampling_rate = -1) -> nanoseconds;

	// Convert a duration to a number of full audio samples.
	[[nodiscard]] auto ns_to_samples(nanoseconds, int sampling_rate = -1) -> ssize_t;

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

// Converts LUFS relative to target to an amplitude gain value.
[[nodiscard]] auto lufs_to_gain(double lufs) -> float;

}

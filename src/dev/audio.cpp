/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "dev/audio.hpp"

#include "preamble.hpp"

namespace playnote::dev {

Audio::Audio(Logger::Category cat, function<void(span<Sample>)> generator):
	cat{cat},
	generator{move(generator)}
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

Audio::~Audio() noexcept
{
#ifdef TARGET_LINUX
	lib::pw::cleanup(move(context));
	INFO_AS(cat, "Pipewire audio cleaned up");
#elifdef TARGET_WINDOWS
	lib::wasapi::cleanup(move(context));
	INFO_AS(cat, "WASAPI audio cleaned up");
#endif
}

auto Audio::samples_to_ns(ssize_t samples, int sampling_rate) -> nanoseconds
{
	auto const rate = sampling_rate == -1? context->properties.sampling_rate : sampling_rate;
	ASSERT(rate > 0);
	auto const ns_per_sample = duration_cast<nanoseconds>(duration<double>{1.0 / rate});
	auto const whole_seconds = samples / rate;
	auto const remainder = samples % rate;
	return 1s * whole_seconds + ns_per_sample * remainder;
}

auto Audio::ns_to_samples(nanoseconds ns, int sampling_rate) -> ssize_t
{
	auto const rate = sampling_rate == -1? context->properties.sampling_rate : sampling_rate;
	ASSERT(rate > 0);
	auto const ns_per_sample = duration_cast<nanoseconds>(duration<double>{1.0 / rate});
	return ns / ns_per_sample;
}

auto lufs_to_gain(double lufs) -> float
{
	constexpr auto LufsTarget = -14.0;
	auto const db_from_target = LufsTarget - lufs;
	auto const amplitude_ratio = pow(10.0, db_from_target / 20.0);
	return static_cast<float>(amplitude_ratio);
}

}

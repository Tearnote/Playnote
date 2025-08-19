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

class Audio {
public:
	static constexpr auto ChannelCount = 2zu;
	static constexpr auto Latency = 128zu;

	template<callable<void(span<Sample>)> Func>
	explicit Audio(Func&& generator);
	~Audio();

	// Return current sampling rate. The value is only valid while an Audio instance exists.
	[[nodiscard]] static auto get_sampling_rate() -> uint32 { return ASSERT_VAL(sampling_rate.load()); }

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

	static inline atomic<uint32> sampling_rate = 0;

#ifndef _WIN32
	lib::pw::ThreadLoop loop;
	lib::pw::Stream stream;
#else
	lib::wasapi::Context context;
#endif

	function<void(span<Sample>)> generator;

	static void on_process(void*);
#ifndef _WIN32
	static void on_param_changed(void*, uint32_t, lib::pw::SPAPod);
#endif
};

template<callable<void(span<Sample>)> Func>
Audio::Audio(Func&& generator):
	generator{generator}
{
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

#ifndef _WIN32
	auto& stream = self.stream;
	auto buffer_opt = lib::pw::dequeue_buffer(stream);
	if (!buffer_opt) return;
	auto& [buffer, request] = buffer_opt.value();
#else
	auto buffer = lib::wasapi::dequeue_buffer(self.context);
#endif

	self.generator(buffer);

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

/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

dev/audio.cppm:
Initializes audio and submits buffers for playback.
*/

module;
#include "macros/logger.hpp"
#include "macros/assert.hpp"

export module playnote.dev.audio;

import playnote.preamble;
import playnote.config;
import playnote.logger;
import playnote.lib.pipewire;

namespace playnote::dev {

namespace pw = lib::pw;
export using Sample = pw::Sample;

export class Audio {
public:
	static constexpr auto ChannelCount = 2u;
	static constexpr auto Latency = 256u;

	Audio();
	~Audio();

	[[nodiscard]] static auto get_sampling_rate() noexcept -> uint32 { return ASSERT_VAL(sampling_rate); }

	template<typename T>
	void add_generator(T& generator) { generators.emplace(&generator, [&]() { return generator.next_sample(); }); }

	template<typename T>
	void remove_generator(T& generator) { generators.erase(&generator); }

	Audio(Audio const&) = delete;
	auto operator=(Audio const&) -> Audio& = delete;
	Audio(Audio&&) = delete;
	auto operator=(Audio&&) -> Audio& = delete;

private:
	InstanceLimit<Audio, 1> instance_limit;

	static inline atomic<uint32> sampling_rate = 0;

	pw::ThreadLoop loop;
	pw::Stream stream;

	unordered_map<void*, function<Sample()>> generators;

	static void on_process(void*) noexcept;
	static void on_param_changed(void*, uint32_t, pw::SPAPod) noexcept;
};

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

void Audio::on_process(void* userdata) noexcept
{
	auto& self = *static_cast<Audio*>(userdata);
	auto& stream = self.stream;

	auto buffer_opt = pw::dequeue_buffer(stream);
	if (!buffer_opt) return;
	auto& [buffer, request] = buffer_opt.value();

	for (auto& dest: buffer) {
		dest = {};
		for (auto const& generator: self.generators | views::values) {
			auto const sample = generator();
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

void Audio::on_param_changed(void*, uint32_t id, pw::SPAPod param) noexcept
{
	auto const new_sampling_rate = pw::get_sampling_rate_from_param(id, param);
	if (!new_sampling_rate) return;
	sampling_rate = *new_sampling_rate;
}

}

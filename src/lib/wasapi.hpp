/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/wasapi.hpp:
WASAPI wrapper for Windows audio support.
*/

#pragma once
#include "preamble.hpp"
#include "lib/audio_common.hpp"

// Forward declarations

struct IAudioClient;
struct IAudioRenderClient;

namespace playnote::lib::wasapi {

struct Context_t {
	AudioProperties properties;
	bool exclusive_mode;
	IAudioClient* client;
	IAudioRenderClient* renderer;
	jthread buffer_thread;
	shared_ptr<atomic<bool>> running_signal;
	function<void(span<Sample>)> processor;
};
using Context = unique_ptr<Context_t>;

auto init(bool exclusive_mode, function<void(span<Sample>)>&& processor) -> Context;

void cleanup(Context&& ctx) noexcept;

}

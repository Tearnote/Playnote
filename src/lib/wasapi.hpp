/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "utils/logger.hpp"
#include "lib/audio_common.hpp"

// Forward declarations

struct IAudioClient;
struct IAudioRenderClient;

namespace playnote::lib::wasapi {

// Context object for WASAPI internal state.
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

// Initialize WASAPI and open an audio stream. processor function will be called in a separate
// thread with a buffer of samples to fill. A Context is returned and must be passed to cleanup().
// If latency is not specified, the smallest possible latency is used.
// Throws runtime_error on failure.
auto init(Logger::Category, bool exclusive_mode, function<void(span<Sample>)>&& processor, optional<nanoseconds> latency) -> Context;

// Clean up WASAPI and associated objects.
void cleanup(Context&& ctx) noexcept;

}

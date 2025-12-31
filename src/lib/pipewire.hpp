/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "lib/audio_common.hpp"

// Forward declarations

struct pw_thread_loop;

namespace playnote::lib::pw {

// Forward declarations

struct Stream_t;

// Context object for PipeWire internal state.
struct Context_t {
	AudioProperties properties;
	pw_thread_loop* loop;
	Stream_t* stream;
	function<void(span<Sample>)> processor;
};
using Context = unique_ptr<Context_t>;

// Initialize PipeWire and open an audio stream. processor function will be called in a separate
// thread with a buffer of samples to fill. A Context is returned and must be passed to cleanup().
// Throws system_error on failure.
auto init(string_view stream_name, int buffer_size, function<void(span<Sample>)>&& processor) -> Context;

// Clean up PipeWire and associated objects.
void cleanup(Context&& context);

}

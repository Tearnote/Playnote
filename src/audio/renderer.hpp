/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "dev/audio.hpp"
#include "bms/cursor.hpp"
#include "bms/chart.hpp"

namespace playnote::audio {

// An "offline" equivalent to audio/player. Instruments a cursor with sample playback, driven manually.
class Renderer {
public:
	// Create a renderer for the given chart.
	explicit Renderer(shared_ptr<bms::Chart const>);

	[[nodiscard]] auto get_cursor() const -> bms::Cursor const& { return *cursor; }

	// Directly change the cursor's position, without triggering any sounds between current position
	// and seek position. All playing sounds are stopped.
	void seek(nanoseconds);

	// Advance chart playback by one audio sample. If nullopt is returned, the chart has ended.
	auto advance_one_sample() -> optional<dev::Sample>;

private:
	struct ActiveSound {
		ssize_t channel;
		span<dev::Sample const> audio;
		ssize_t position;
	};
	shared_ptr<bms::Chart const> chart;
	shared_ptr<bms::Cursor> cursor;
	small_vector<ActiveSound, 128> active_sounds;
};

}

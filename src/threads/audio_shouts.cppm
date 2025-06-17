/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/audio_events.cppm:
Shouts that can be spawned by the audio thread.
*/

export module playnote.threads.audio_shouts;

import playnote.preamble;
import playnote.bms.audio_player;

namespace playnote::threads {

export struct ChartLoadProgress {
	struct Build {};
	struct FileLoad {
		string filename;
		usize index;
		usize total;
	};
	struct LoudnessCalc {
		nanoseconds progress;
	};
	struct Finished {
		weak_ptr<bms::AudioPlayer const> player;
	};
	using Type = variant<monostate, Build, FileLoad, LoudnessCalc, Finished>;
	Type type;

	// Shouts must be default-constructible
	ChartLoadProgress() = default;

	template<variant_alternative<Type> T>
	explicit ChartLoadProgress(T&& type): type{forward<T>(type)} {}
};

}

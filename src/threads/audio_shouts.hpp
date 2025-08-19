/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/audio_events.hpp:
Shouts that can be spawned by the audio thread.
*/

#pragma once
#include "preamble.hpp"
#include "audio/player.hpp"

namespace playnote::threads {

struct ChartLoadProgress {
	struct CompilingIR {
		fs::path chart_path;
	};
	struct Building {
		fs::path chart_path;
	};
	struct LoadingFiles {
		fs::path chart_path;
		usize loaded;
		usize total;
	};
	struct Measuring {
		fs::path chart_path;
		nanoseconds progress;
	};
	struct DensityCalculation {
		fs::path chart_path;
		nanoseconds progress;
	};
	struct Finished {
		fs::path chart_path;
		weak_ptr<audio::Player const> player;
	};
	struct Failed {
		fs::path chart_path;
		string message;
	};
	using Type = variant<monostate, CompilingIR, Building, LoadingFiles, Measuring, DensityCalculation, Finished, Failed>;
	Type type;

	// Shouts must be default-constructible
	ChartLoadProgress() = default;

	explicit ChartLoadProgress(Type const& type): type{type} {}
	explicit ChartLoadProgress(Type&& type): type{forward<Type>(type)} {}

	template<variant_alternative<Type> T>
	explicit ChartLoadProgress(T&& type): type{forward<T>(type)} {}
};

}

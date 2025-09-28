/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

config.hpp:
Global game configuration, both compile-time and runtime.
*/

#pragma once
#include "preamble.hpp"
#include "service.hpp"
#include "logger.hpp"

namespace playnote {
inline constexpr auto AppTitle = "Playnote";
inline constexpr auto AppVersion = to_array({0u, 0u, 3u});

#if !defined(BUILD_DEBUG) && !defined(BUILD_RELDEB) && !defined(BUILD_RELEASE)
#error Build type incorrectly defined
#endif

#ifdef _WIN32
#define TARGET_WINDOWS
#elifdef __linux__
#define TARGET_LINUX
#else
#error Unknown target platform
#endif

// Logfile location
#ifdef BUILD_DEBUG
inline constexpr auto LogfilePath = "playnote-debug.log"sv;
#elifdef BUILD_RELDEB
inline constexpr auto LogfilePath = "playnote-reldeb.log"sv;
#else
inline constexpr auto LogfilePath = "playnote.log"sv;
#endif

// Config file location
inline constexpr auto ConfigPath = "config.toml"sv;

// Song library database locations

inline constexpr auto LibraryPath = "library"sv;
inline constexpr auto LibraryDBPath = "library.db"sv;

// Global runtime configuration, kept in sync with the config file.
class Config {
public:
	using Value = variant<int32, double, bool, string>;
	struct Entry {
		string category;
		string name;
		Value value;
	};

	// Create the config object, with entries at their default values.
	Config() { create_defaults(); }

	// Overwrite the config file with current entries.
	~Config() noexcept;

	// Update all entries with values from the config file.
	void load_from_file();

	// Flush the config to file, overwriting it.
	void save_to_file() const;

	// Get the value of an entry.
	template <variant_alternative<Value> T>
	[[nodiscard]] auto get_entry(string_view category, string_view name) const -> T const&
	{
		return get<T>(find_entry(category, name).value);
	}

	// Set an entry to a new value.
	void set_entry(Entry&&);

	Config(Config const&) = delete;
	auto operator=(Config const&) -> Config& = delete;
	Config(Config&&) = delete;
	auto operator=(Config&&) -> Config& = delete;

private:
	vector<Entry> entries;

	[[nodiscard]] auto find_entry(string_view category, string_view name) -> Entry&;
	[[nodiscard]] auto find_entry(string_view category, string_view name) const -> Entry const&;
	void create_defaults();
};

// Consult this function for the list of registered config entries.
inline void Config::create_defaults()
{
	entries.emplace_back(Entry{
		.category = "system",
		.name = "attach_console",
		.value = false,
	});

	entries.emplace_back(Entry{
		.category = "logging",
		.name = "global",
		.value = "Info",
	});
	entries.emplace_back(Entry{
		.category = "logging",
		.name = "bms_build",
		.value = "Info",
	});
	entries.emplace_back(Entry{
		.category = "logging",
		.name = "graphics",
		.value = "Info",
	});

	entries.emplace_back(Entry{
		.category = "pipewire",
		.name = "buffer_size",
		.value = 128,
	});

	entries.emplace_back(Entry{
		.category = "wasapi",
		.name = "exclusive_mode",
		.value = true,
	});
	entries.emplace_back(Entry{
		.category = "wasapi",
		.name = "use_custom_latency",
		.value = false,
	});
	entries.emplace_back(Entry{
		.category = "wasapi",
		.name = "custom_latency",
		.value = 10,
	});

	entries.emplace_back(Entry{
		.category = "vulkan",
		.name = "present_mode",
		.value = "Immediate",
	});
	entries.emplace_back(Entry{
		.category = "vulkan",
		.name = "frames_in_flight",
		.value = 1,
	});
	entries.emplace_back(Entry{
		.category = "vulkan",
		.name = "validation_enabled",
		.value = false,
	});

	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_5k_1",
		.value = "Z",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_5k_2",
		.value = "S",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_5k_3",
		.value = "X",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_5k_4",
		.value = "D",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_5k_5",
		.value = "C",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_5k_s",
		.value = "LeftShift",
	});

	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_7k_1",
		.value = "Z",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_7k_2",
		.value = "S",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_7k_3",
		.value = "X",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_7k_4",
		.value = "D",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_7k_5",
		.value = "C",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_7k_6",
		.value = "F",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_7k_7",
		.value = "V",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_7k_s",
		.value = "LeftShift",
	});

	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p1_1",
		.value = "Z",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p1_2",
		.value = "S",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p1_3",
		.value = "X",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p1_4",
		.value = "D",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p1_5",
		.value = "C",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p1_s",
		.value = "LeftShift",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p2_1",
		.value = "M",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p2_2",
		.value = "K",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p2_3",
		.value = "Comma",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p2_4",
		.value = "L",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p2_5",
		.value = "Period",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p2_s",
		.value = "RightShift",
	});

	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p1_1",
		.value = "Z",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p1_2",
		.value = "S",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p1_3",
		.value = "X",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p1_4",
		.value = "D",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p1_5",
		.value = "C",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p1_6",
		.value = "F",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p1_7",
		.value = "V",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p1_s",
		.value = "LeftShift",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p2_1",
		.value = "M",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p2_2",
		.value = "K",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p2_3",
		.value = "Comma",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p2_4",
		.value = "L",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p2_5",
		.value = "Period",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p2_6",
		.value = "Semicolon",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p2_7",
		.value = "Slash",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p2_s",
		.value = "RightShift",
	});

	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_5k_1",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_5k_2",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_5k_3",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_5k_4",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_5k_5",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_5k_s",
		.value = "None",
	});

	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_7k_1",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_7k_2",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_7k_3",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_7k_4",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_7k_5",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_7k_6",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_7k_7",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_7k_s",
		.value = "None",
	});

	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p1_1",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p1_2",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p1_3",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p1_4",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p1_5",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p1_s",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p2_1",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p2_2",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p2_3",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p2_4",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p2_5",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p2_s",
		.value = "None",
	});

	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p1_1",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p1_2",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p1_3",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p1_4",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p1_5",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p1_6",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p1_7",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p1_s",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p2_1",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p2_2",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p2_3",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p2_4",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p2_5",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p2_6",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p2_7",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p2_s",
		.value = "None",
	});

	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_5k_s_analog",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_7k_s_analog",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p1_s_analog",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p2_s_analog",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p1_s_analog",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p2_s_analog",
		.value = "None",
	});

	entries.emplace_back(Entry{
		.category = "controls",
		.name = "debounce_duration",
		.value = 4,
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "turntable_stop_timeout",
		.value = 200,
	});

	entries.emplace_back(Entry{
		.category = "gameplay",
		.name = "scroll_speed",
		.value = 3.0,
	});
	entries.emplace_back(Entry{
		.category = "gameplay",
		.name = "note_offset",
		.value = 0,
	});
	entries.emplace_back(Entry{
		.category = "gameplay",
		.name = "judgment_timeout",
		.value = 400,
	});
}

namespace globals {
inline auto config = Service<Config>{};
}

}

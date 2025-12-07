/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "utils/service.hpp"

namespace playnote {
inline constexpr auto AppTitle = "Playnote";
inline constexpr auto AppVersion = to_array({0u, 0u, 4u});

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
	using Value = variant<int, double, bool, string>;
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
	{ return get<T>(find_entry(category, name).value); }

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

namespace globals {
inline auto config = Service<Config>{};
}

}

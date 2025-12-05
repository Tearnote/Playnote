/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "lib/sqlite.hpp"
#include "dev/audio.hpp"
#include "io/source.hpp"
#include "io/file.hpp"

namespace playnote::io {

// An archive optimized for file lookup and zero-copy access. Once opened, the contents are immutable.
class Song {
public:
	// Create from an existing songzip.
	explicit Song(Logger::Category, ReadFile&&);

	// Convert from a Source.
	static auto from_source(Logger::Category, unique_ptr<thread_pool>&,
		Source const&, fs::path const& dst) -> task<Song>;

	// Convert from a Source, using an existing songzip as base.
	static auto from_source_append(Logger::Category, unique_ptr<thread_pool>&,
		ReadFile&& src, Source const& ext, fs::path const& dst) -> task<Song>;

	// Return all charts of the song.
	auto for_each_chart() -> generator<tuple<string_view, span<byte const>>>;

	// Load the requested file.
	auto load_file(string_view filepath) -> span<byte const>;

	// Preload all audio files to an internal cache. This cache will be used in any later load_audio_file() calls.
	// The loads are performed in parallel. Useful when loading multiple charts of the same song.
	auto preload_audio_files(unique_ptr<thread_pool>&, int sampling_rate) -> task<>;

	// Load the requested audio file, decode it, and resample to current device sample rate.
	auto load_audio_file(string_view filepath, int sampling_rate) -> vector<dev::Sample>;

	// Destroy the song and delete the underlying songzip from disk.
	void remove() && noexcept;

private:
	static constexpr auto ContentsSchema = to_array({R"sql(
		CREATE TABLE contents(
			path TEXT NOT NULL COLLATE nocase,
			type INTEGER NOT NULL,
			ptr BLOB NOT NULL,
			size INTEGER NOT NULL
		)
	)sql"sv, R"sql(
		CREATE INDEX contents_path ON contents(path)
	)sql"sv});
	struct InsertContents {
		static constexpr auto Query = R"sql(
			INSERT INTO contents(path, type, ptr, size) VALUES (?1, ?2, ?3, ?4)
		)sql"sv;
		using Params = tuple<string_view, int, void const*, isize_t>;
	};
	struct SelectCharts {
		static constexpr auto Query = R"sql(
			SELECT path, ptr, size FROM contents WHERE type = 1
		)sql"sv;
		using Row = tuple<string_view, void const*, isize_t>;
	};
	struct SelectFile {
		static constexpr auto Query = R"sql(
			SELECT ptr, size FROM contents WHERE path = ?1
		)sql"sv;
		using Params = tuple<string_view>;
		using Row = tuple<void const*, isize_t>;
	};
	struct SelectAudioFile {
		static constexpr auto Query = R"sql(
			SELECT ptr, size FROM contents WHERE type = 2 AND path = ?1
		)sql"sv;
		using Params = tuple<string_view>;
		using Row = tuple<void const*, isize_t>;
	};
	struct SelectAudioFiles {
		static constexpr auto Query = R"sql(
			SELECT path, ptr, size FROM contents WHERE type = 2
		)sql"sv;
		using Row = tuple<string_view, void const*, isize_t>;
	};

	Logger::Category cat;
	ReadFile file;
	lib::sqlite::DB db;
	lib::sqlite::Statement<SelectCharts> select_charts;
	lib::sqlite::Statement<SelectFile> select_file;
	lib::sqlite::Statement<SelectAudioFiles> select_audio_files;
	unordered_map<string, vector<dev::Sample>, string_hash> audio_cache;
};

}

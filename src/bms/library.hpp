/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "utils/logger.hpp"
#include "lib/sqlite.hpp"
#include "io/song.hpp"
#include "bms/chart.hpp"

namespace playnote::bms {

// Database of all available charts. Searchable, and can import new charts from folders and archives.
class Library {
public:
	// Minimal metadata about a chart in the library.
	struct ChartEntry {
		MD5 md5;
		string title;
	};

	// Open an existing library, or create an empty one at the provided path.
	// The thread pool passed in will be used for import jobs.
	Library(Logger::Category, unique_ptr<thread_pool>&, fs::path const&);
	~Library() noexcept;

	// Import a song and all its charts into the library. Returns instantly; the import happens in the background.
	// All other methods are safe to call while an import is in progress.
	void import(fs::path const&);

	// Return true if an import is ongoing.
	[[nodiscard]] auto is_importing() const -> bool { return !import_tasks.empty(); }

	// Return a list of all available charts. Thread-safe.
	[[nodiscard]] auto list_charts() -> task<vector<ChartEntry>>;

	// Return true if the library has changed since the last call to list_charts().
	[[nodiscard]] auto is_dirty() const -> bool { return dirty.load(); }

	// Return the number of songs that were imported so far.
	[[nodiscard]] auto get_import_songs_processed() const -> ssize_t { return import_stats.songs_processed.load(); }

	// Return the number of songs discovered during import so far.
	[[nodiscard]] auto get_import_songs_total() const -> ssize_t { return import_stats.songs_total.load(); }

	// Return the number of songs that failed to import.
	[[nodiscard]] auto get_import_songs_failed() const -> ssize_t { return import_stats.songs_failed.load(); }

	// Return the number of charts that were imported so far.
	[[nodiscard]] auto get_import_charts_added() const -> ssize_t { return import_stats.charts_added.load(); }

	// Return the number of charts that were skipped as duplicates.
	[[nodiscard]] auto get_import_charts_skipped() const -> ssize_t { return import_stats.charts_skipped.load(); }

	// Return the number of charts that failed to import.
	[[nodiscard]] auto get_import_charts_failed() const -> ssize_t { return import_stats.charts_failed.load(); }

	// Set all import statistics to zero. Can be used during an import, but the values might be inconsistent afterwards.
	void reset_import_stats();

	// Load a chart from the library.
	auto load_chart(unique_ptr<thread_pool>&, MD5) -> task<shared_ptr<Chart const>>;

	Library(Library const&) = delete;
	auto operator=(Library const&) -> Library& = delete;
	Library(Library&&) = delete;
	auto operator=(Library&&) -> Library& = delete;

private:
	static constexpr auto SongsSchema = R"sql(
		CREATE TABLE IF NOT EXISTS songs(
			id INTEGER PRIMARY KEY,
			path TEXT NOT NULL UNIQUE
		)
	)sql"sv;
	struct SongExists {
		static constexpr auto Query = R"sql(
			SELECT 1 FROM songs WHERE path = ?1
		)sql"sv;
		using Params = tuple<string_view>;
	};
	struct SelectSongByID {
		static constexpr auto Query = R"sql(
			SELECT path FROM songs WHERE id = ?1
		)sql"sv;
		using Params = tuple<ssize_t>;
		using Row = tuple<string_view>;
	};
	struct InsertSong {
		static constexpr auto Query = R"sql(
			INSERT INTO songs(path) VALUES(?1)
		)sql"sv;
		using Params = tuple<string_view>;
	};
	struct DeleteSong {
		static constexpr auto Query = R"sql(
			DELETE FROM songs WHERE id = ?1
		)sql"sv;
		using Params = tuple<ssize_t>;
	};

	static constexpr auto ChartsSchema = to_array({R"sql(
		CREATE TABLE IF NOT EXISTS charts(
			md5 BLOB PRIMARY KEY NOT NULL CHECK(length(md5) == 16),
			song_id INTEGER NOT NULL REFERENCES songs ON DELETE CASCADE,
			path TEXT NOT NULL,
			date_imported INTEGER DEFAULT(unixepoch()),
			title TEXT NOT NULL,
			subtitle TEXT,
			artist TEXT,
			subartist TEXT,
			genre TEXT,
			url TEXT,
			email TEXT,
			difficulty INTEGER NOT NULL CHECK(difficulty >= 0 AND difficulty <= 5),
			playstyle INTEGER NOT NULL CHECK(playstyle >= 0 AND playstyle <= 4),
			has_ln BOOLEAN NOT NULL,
			has_soflan BOOLEAN NOT NULL,
			note_count INTEGER NOT NULL CHECK(note_count >= 0),
			chart_duration INTEGER NOT NULL CHECK(chart_duration >= 0),
			audio_duration INTEGER NOT NULL CHECK(audio_duration >= 0),
			loudness REAL NOT NULL,
			average_nps REAL NOT NULL CHECK(average_nps >= 0),
			peak_nps REAL NOT NULL CHECK(peak_nps >= 0),
			min_bpm REAL NOT NULL,
			max_bpm REAL NOT NULL,
			main_bpm REAL NOT NULL,
			preview_id INTEGER REFERENCES chart_previews
		)
	)sql"sv, R"sql(
		CREATE INDEX IF NOT EXISTS charts_title ON charts(title)
	)sql"sv, R"sql(
		CREATE INDEX IF NOT EXISTS charts_subtitle ON charts(subtitle)
	)sql"sv, R"sql(
		CREATE INDEX IF NOT EXISTS charts_artist ON charts(artist)
	)sql"sv, R"sql(
		CREATE INDEX IF NOT EXISTS charts_subartist ON charts(subartist)
	)sql"sv, R"sql(
		CREATE INDEX IF NOT EXISTS charts_genre ON charts(genre)
	)sql"sv, R"sql(
		CREATE INDEX IF NOT EXISTS charts_difficulty ON charts(difficulty)
	)sql"sv, R"sql(
		CREATE INDEX IF NOT EXISTS charts_playstyle ON charts(playstyle)
	)sql"sv, R"sql(
		CREATE INDEX IF NOT EXISTS charts_note_count ON charts(note_count)
	)sql"sv, R"sql(
		CREATE INDEX IF NOT EXISTS charts_chart_duration ON charts(chart_duration)
	)sql"sv, R"sql(
		CREATE INDEX IF NOT EXISTS charts_average_nps ON charts(average_nps)
	)sql"sv, R"sql(
		CREATE INDEX IF NOT EXISTS charts_peak_nps ON charts(peak_nps)
	)sql"sv, R"sql(
		CREATE INDEX IF NOT EXISTS charts_main_bpm ON charts(main_bpm)
	)sql"sv});
	struct ChartExists {
		static constexpr auto Query = R"sql(
			SELECT 1 FROM charts WHERE md5 = ?1
		)sql"sv;
		using Params = tuple<span<byte const>>;
	};
	struct ChartListing {
		static constexpr auto Query = R"sql(
			SELECT md5, title, playstyle, difficulty FROM charts
		)sql"sv;
		using Row = tuple<span<byte const>, string_view, int, int>;
	};
	struct InsertChart {
		static constexpr auto Query = R"sql(
			INSERT INTO charts(md5, song_id, path, title, subtitle, artist, subartist, genre, url,
				email, difficulty, playstyle, has_ln, has_soflan, note_count, chart_duration,
				audio_duration, loudness, average_nps, peak_nps, min_bpm, max_bpm, main_bpm, preview_id)
				VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19, ?20, ?21, ?22, ?23, ?24)
		)sql"sv;
		using Params = tuple<span<byte const>, ssize_t, string_view, string_view, string_view, string_view, string_view, string_view, string_view,
			string_view, int, int, int, int, int, int64_t,
			int64_t, double, double, double, double, double, double, ssize_t>;
	};

	static constexpr auto ChartDensitiesSchema = to_array({R"sql(
		CREATE TABLE IF NOT EXISTS chart_densities(
			md5 BLOB UNIQUE NOT NULL REFERENCES charts ON DELETE CASCADE,
			resolution INTEGER NOT NULL CHECK(resolution >= 1),
			key BLOB NOT NULL,
			scratch BLOB NOT NULL,
			ln BLOB NOT NULL
		)
	)sql"sv, R"sql(
		CREATE INDEX IF NOT EXISTS chart_densities_md5 ON chart_densities(md5)
	)sql"sv});
	struct InsertChartDensity {
		static constexpr auto Query = R"sql(
			INSERT INTO chart_densities(md5, resolution, key, scratch, ln) VALUES(?1, ?2, ?3, ?4, ?5)
		)sql"sv;
		using Params = tuple<span<byte const>, int, span<byte const>, span<byte const>, span<byte const>>;
	};

	static constexpr auto ChartImportLogsSchema = R"sql(
		CREATE TABLE IF NOT EXISTS chart_import_logs(
			md5 BLOB UNIQUE NOT NULL REFERENCES charts ON DELETE CASCADE,
			log TEXT NOT NULL
		)
	)sql"sv;
	struct InsertChartImportLog {
		static constexpr auto Query = R"sql(
			INSERT INTO chart_import_logs(md5, log) VALUES(?1, ?2)
		)sql"sv;
		using Params = tuple<span<byte const>, string_view>;
	};

	static constexpr auto ChartPreviewsSchema = R"sql(
		CREATE TABLE IF NOT EXISTS chart_previews(
			id INTEGER PRIMARY KEY,
			preview BLOB NOT NULL
		)
	)sql"sv;
	struct InsertChartPreview {
		static constexpr auto Query = R"sql(
			INSERT INTO chart_previews(preview) VALUES(?1)
		)sql"sv;
		using Params = tuple<span<byte const>>;
	};
	struct DeleteChartPreview {
		static constexpr auto Query = R"sql(
			DELETE FROM chart_previews WHERE id = ?1
		)sql"sv;
		using Params = tuple<ssize_t>;
	};
	struct SelectChartPreviewIDs {
		static constexpr auto Query = R"sql(
			SELECT preview_id FROM charts WHERE md5 = ?1
		)sql"sv;
		using Params = tuple<span<byte const>>;
		using Row = tuple<ssize_t>;
	};
	struct ModifyChartPreviewIDs {
		static constexpr auto Query = R"sql(
			UPDATE charts SET preview_id = ?2 WHERE preview_id = ?1
		)sql"sv;
		using Params = tuple<ssize_t, ssize_t>;
	};
	struct SelectSongPreviews {
		static constexpr auto Query = R"sql(
			SELECT chart_previews.id, chart_previews.preview FROM chart_previews
				INNER JOIN charts ON chart_previews.id = charts.preview_id
				WHERE charts.song_id = ?1
		)sql"sv;
		using Params = tuple<ssize_t>;
		using Row = tuple<ssize_t, span<byte const>>;
	};

	struct GetSongFromChart {
		static constexpr auto Query = R"sql(
			SELECT songs.id, songs.path FROM songs INNER JOIN charts ON songs.id = charts.song_id WHERE charts.md5 = ?1
		)sql"sv;
		using Params = tuple<span<byte const>>;
		using Row = tuple<ssize_t, string_view>;
	};
	struct SelectSongChart {
		static constexpr auto Query = R"sql(
			SELECT
				songs.path, charts.path, charts.date_imported, charts.title, charts.subtitle, charts.artist,
				charts.subartist, charts.genre, charts.url, charts.email, charts.difficulty, charts.playstyle,
				charts.has_ln, charts.has_soflan, charts.note_count, charts.chart_duration, charts.audio_duration,
				charts.loudness, charts.average_nps, charts.peak_nps, charts.min_bpm, charts.max_bpm, charts.main_bpm,
				chart_densities.resolution, chart_densities.key, chart_densities.scratch, chart_densities.ln
				FROM charts
				INNER JOIN songs ON songs.id = charts.song_id
				INNER JOIN chart_densities ON charts.md5 = chart_densities.md5
				WHERE charts.md5 = ?1
		)sql"sv;
		using Params = tuple<span<byte const>>;
		using Row = tuple<string_view, string_view, int64_t, string_view, string_view, string_view,
			string_view, string_view, string_view, string_view, int, int,
			int, int, int, int64_t, int64_t,
			double, double, double, double, double, double,
			int, span<byte const>, span<byte const>, span<byte const>>;
	};

	struct ImportStats {
		atomic<ssize_t> songs_processed = 0;
		atomic<ssize_t> songs_total = 0;
		atomic<ssize_t> songs_failed = 0;
		atomic<ssize_t> charts_added = 0;
		atomic<ssize_t> charts_skipped = 0;
		atomic<ssize_t> charts_failed = 0;
	};

	Logger::Category cat;
	unique_ptr<thread_pool>& pool;

	lib::sqlite::DB db;
	task_container import_tasks;
	unordered_map<MD5, ssize_t> staging;
	coro_mutex staging_lock;
	unordered_node_map<ssize_t, coro_mutex> song_locks;
	atomic<bool> dirty = true;
	atomic<bool> stopping = false;
	ImportStats import_stats;

	[[nodiscard]] auto find_available_song_filename(string_view name) -> string;
	auto import_many(fs::path) -> task<>;
	auto import_one(fs::path) -> task<>;
	auto import_chart(io::Song& song, ssize_t song_id, string chart_path, span<byte const>) -> task<MD5>;
	auto deduplicate_previews(ssize_t song_id, span<MD5 const> new_charts) -> task<ssize_t>;
};

}

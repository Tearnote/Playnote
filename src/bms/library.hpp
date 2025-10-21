/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/library.hpp:
A cache of song and chart metadata. Handles import events.
*/

#pragma once
#include "preamble.hpp"
#include "utils/task_pool.hpp"
#include "utils/config.hpp"
#include "utils/logger.hpp"
#include "lib/openssl.hpp"
#include "lib/sqlite.hpp"
#include "lib/bits.hpp"
#include "io/song.hpp"
#include "bms/builder.hpp"
#include "bms/chart.hpp"

namespace playnote::bms {

class Library {
public:
	// Minimal metadata about a chart in the library.
	struct ChartEntry {
		MD5 md5;
		string title;
	};

	// Open an existing library, or create an empty one at the provided path.
	explicit Library(Logger::Category, fs::path const&);
	~Library();

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
	[[nodiscard]] auto get_import_songs_processed() const -> isize { return import_stats.songs_processed.load(); }

	// Return the number of songs discovered during import so far.
	[[nodiscard]] auto get_import_songs_total() const -> isize { return import_stats.songs_total.load(); }

	// Return the number of songs that failed to import.
	[[nodiscard]] auto get_import_songs_failed() const -> isize { return import_stats.songs_failed.load(); }

	// Return the number of charts that were imported so far.
	[[nodiscard]] auto get_import_charts_added() const -> isize { return import_stats.charts_added.load(); }

	// Return the number of charts that were skipped as duplicates.
	[[nodiscard]] auto get_import_charts_skipped() const -> isize { return import_stats.charts_skipped.load(); }

	// Return the number of charts that failed to import.
	[[nodiscard]] auto get_import_charts_failed() const -> isize { return import_stats.charts_failed.load(); }

	// Set all import statistics to zero. Can be used during an import, but the values might be inconsistent afterwards.
	void reset_import_stats();

	// Load a chart from the library.
	auto load_chart(MD5) -> task<shared_ptr<Chart const>>;

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
	static constexpr auto SongExistsQuery = R"sql(
		SELECT 1 FROM songs WHERE path = ?1
	)sql"sv;
	static constexpr auto InsertSongQuery = R"sql(
		INSERT INTO songs(path) VALUES(?1)
	)sql"sv;
	static constexpr auto DeleteSongQuery = R"sql(
		DELETE FROM songs WHERE id = ?1
	)sql"sv;

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
			main_bpm REAL NOT NULL
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
	static constexpr auto ChartExistsQuery = R"sql(
		SELECT 1 FROM charts WHERE md5 = ?1
	)sql"sv;
	static constexpr auto ChartListingQuery = R"sql(
		SELECT md5, title, playstyle, difficulty FROM charts
	)sql"sv;
	static constexpr auto InsertChartQuery = R"sql(
		INSERT INTO charts(md5, song_id, path, title, subtitle, artist, subartist, genre, url,
			email, difficulty, playstyle, has_ln, has_soflan, note_count, chart_duration,
			audio_duration, loudness, average_nps, peak_nps, min_bpm, max_bpm, main_bpm)
			VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19, ?20, ?21, ?22, ?23)
	)sql"sv;

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
	static constexpr auto InsertChartDensityQuery = R"sql(
		INSERT INTO chart_densities(md5, resolution, key, scratch, ln) VALUES(?1, ?2, ?3, ?4, ?5)
	)sql"sv;

	static constexpr auto ChartImportLogsSchema = R"sql(
		CREATE TABLE IF NOT EXISTS chart_import_logs(
			md5 BLOB UNIQUE NOT NULL REFERENCES charts ON DELETE CASCADE,
			log TEXT NOT NULL
		)
	)sql"sv;
	static constexpr auto InsertChartImportLogQuery = R"sql(
		INSERT INTO chart_import_logs(md5, log) VALUES(?1, ?2)
	)sql"sv;

	static constexpr auto GetSongFromChartQuery = R"sql(
		SELECT songs.id, songs.path FROM songs INNER JOIN charts ON songs.id = charts.song_id WHERE charts.md5 = ?1
	)sql"sv;
	static constexpr auto SelectSongChartQuery = R"sql(
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

	struct ImportStats {
		atomic<isize> songs_processed = 0;
		atomic<isize> songs_total = 0;
		atomic<isize> songs_failed = 0;
		atomic<isize> charts_added = 0;
		atomic<isize> charts_skipped = 0;
		atomic<isize> charts_failed = 0;
	};

	Logger::Category cat;

	lib::sqlite::DB db;
	task_container import_tasks;
	coro_mutex song_lock;
	coro_mutex transaction_lock;
	atomic<bool> dirty = true;
	atomic<bool> stopping = false;
	ImportStats import_stats;

	[[nodiscard]] auto find_available_song_filename(string_view name) -> string;
	auto import_many(fs::path) -> task<>;
	auto import_one(fs::path) -> task<>;
	auto import_song(fs::path const&) -> pair<isize, string>;
	auto import_chart(io::Song& song, isize song_id, string chart_path, span<byte const>) -> task<>;
};

inline Library::Library(Logger::Category cat, fs::path const& path):
	cat{cat},
	db{lib::sqlite::open(path)},
	import_tasks{*globals::task_pool}
{
	lib::sqlite::execute(db, SongsSchema);
	lib::sqlite::execute(db, ChartsSchema);
	lib::sqlite::execute(db, ChartDensitiesSchema);
	lib::sqlite::execute(db, ChartImportLogsSchema);
	fs::create_directory(LibraryPath);
	INFO_AS(cat, "Opened song library at \"{}\"", path);
}

inline Library::~Library()
{
	stopping.store(true);
}

inline void Library::import(fs::path const& path)
{
	import_tasks.start(import_many(path));
}

inline auto Library::list_charts() -> task<vector<ChartEntry>>
{
	auto chart_listing = lib::sqlite::prepare(db, ChartListingQuery);
	auto result = vector<ChartEntry>{};
	lib::sqlite::query(chart_listing, [&](span<byte const> md5, string_view title, int playstyle, int difficulty) {
		auto entry = ChartEntry{};
		copy(md5, entry.md5.begin());
		auto const difficulty_str = enum_name(static_cast<Difficulty>(difficulty));
		auto const playstyle_str = enum_name(static_cast<Playstyle>(playstyle));
		entry.title = format("{} [{}] [{}]", title, difficulty_str, playstyle_str.substr(1));
		result.emplace_back(move(entry));
	});
	dirty.store(false);
	co_return result;
}

inline void Library::reset_import_stats()
{
	import_stats.songs_processed.store(0);
	import_stats.songs_total.store(0);
	import_stats.songs_failed.store(0);
	import_stats.charts_added.store(0);
	import_stats.charts_skipped.store(0);
	import_stats.charts_failed.store(0);
}

inline auto Library::load_chart(MD5 md5) -> task<shared_ptr<Chart const>>
{
	auto cache = optional<Metadata>{nullopt};
	auto song_path = fs::path{};
	auto chart_path = string{};
	auto select_song_chart = lib::sqlite::prepare(db, SelectSongChartQuery);
	lib::sqlite::query(select_song_chart, [&](
		string_view song_path_sv, string_view chart_path_sv, [[maybe_unused]] int64 date_imported, string_view title, string_view subtitle,
		string_view artist, string_view subartist, string_view genre, string_view url, string_view email,
		int difficulty, int playstyle, int has_ln, int has_soflan, int note_count, int64 chart_duration,
		int64 audio_duration, double loudness, double average_nps, double peak_nps, double min_bpm, double max_bpm,
		double main_bpm, int density_resolution, span<byte const> density_key, span<byte const> density_scratch,
		span<byte const> density_ln
	) {
		auto deserialize_density = [](span<byte const> v) {
			auto in = lib::bits::in(v);
			auto vec = vector<float>{};
			in(vec).or_throw();
			return vec;
		};
		song_path = fs::path{LibraryPath} / song_path_sv;
		chart_path = chart_path_sv;
		cache = Metadata{
			.title = string{title},
			.subtitle = string{subtitle},
			.artist = string{artist},
			.subartist = string{subartist},
			.genre = string{genre},
			.url = string{url},
			.email = string{email},
			.difficulty = static_cast<Difficulty>(difficulty),
			.playstyle = static_cast<Playstyle>(playstyle),
			.features = Metadata::Features{
				.has_ln = has_ln != 0,
				.has_soflan = has_soflan != 0,
			},
			.note_count = static_cast<uint32>(note_count),
			.chart_duration = nanoseconds{chart_duration},
			.audio_duration = nanoseconds{audio_duration},
			.loudness = loudness,
			.density = Metadata::Density{
				.resolution = nanoseconds{density_resolution},
				.key = deserialize_density(density_key),
				.scratch = deserialize_density(density_scratch),
				.ln = deserialize_density(density_ln),
			},
			.nps = Metadata::NPS{
				.average = static_cast<float>(average_nps),
				.peak = static_cast<float>(peak_nps),
			},
			.bpm_range = Metadata::BPMRange{
				.min = static_cast<float>(min_bpm),
				.max = static_cast<float>(max_bpm),
				.main = static_cast<float>(main_bpm),
			},
		};
		return false;
	}, md5);
	if (!cache) throw runtime_error{"Chart not found"};

	auto song = io::Song::from_zip(song_path);
	auto chart_raw = song.load_file(chart_path);
	auto builder = Builder{cat};
	co_return co_await builder.build(chart_raw, song, *cache);
}

inline auto Library::find_available_song_filename(string_view name) -> string
{
	for (auto i: views::iota(0u)) {
		auto test = i == 0?
			format("{}.zip", name) :
			format("{}-{}.zip", name, i);
		auto exists = false;
		auto song_exists = lib::sqlite::prepare(db, SongExistsQuery);
		lib::sqlite::query(song_exists, [&] { exists = true; }, test);
		if (!exists) return test;
	}
	unreachable();
}

inline auto Library::import_many(fs::path path) -> task<>
{
	if (fs::is_regular_file(path)) {
		import_stats.songs_total.fetch_add(1);
		co_await schedule_task(import_one(path));
	} else if (fs::is_directory(path)) {
		auto contents = vector<fs::directory_entry>{};
		copy(fs::directory_iterator{path}, back_inserter(contents));
		if (any_of(contents, [&](auto const& entry) { return fs::is_regular_file(entry) && io::Song::is_bms_ext(entry.path().extension().string()); })) {
			import_stats.songs_total.fetch_add(1);
			co_await schedule_task(import_one(path));
		} else {
			for (auto const& entry: contents) import_tasks.start(import_many(entry));
		}
	} else {
		ERROR_AS(cat, "Failed to import location \"{}\": unknown type of file", path);
		import_stats.songs_processed.fetch_add(1);
		import_stats.songs_failed.fetch_add(1);
	}
}

inline auto Library::import_one(fs::path path) -> task<>
try {
	if (stopping.load()) {
		WARN_AS(cat, "Song import \"{}\" cancelled", path);
		co_return;
	}
	INFO_AS(cat, "Importing song \"{}\"", path);
	auto lock = co_await song_lock.scoped_lock();
	auto [song_id, song_path] = import_song(path);
	auto song = io::Song::from_zip(song_path);
	song.preload_audio_files();

	auto chart_import_tasks = vector<task<>>{};
	auto chart_paths = vector<string>{};
	song.for_each_chart([&](auto path, auto chart) {
		chart_import_tasks.emplace_back(schedule_task(import_chart(song, song_id, string{path}, chart)));
		chart_paths.emplace_back(path);
	});
	auto results = co_await when_all(move(chart_import_tasks));

	auto imported_count = 0z;
	for (auto [result, path]: views::zip(results, chart_paths)) {
		try {
			result.return_value();
			INFO_AS(cat, "Chart \"{}\" imported successfully", path);
			imported_count += 1;
		} catch (exception const& e) {
			ERROR_AS(cat, "Failed to import chart \"{}\": {}", path, e.what());
			import_stats.charts_failed.fetch_add(1);
		}
	}
	if (imported_count == 0) {
		WARN_AS(cat, "No charts found in song \"{}\"", path);
		auto delete_song = lib::sqlite::prepare(db, DeleteSongQuery);
		lib::sqlite::execute(delete_song, song_id);
		move(song).remove();
	} else {
		INFO_AS(cat, "Song \"{}\" imported successfully", path);
	}
	import_stats.songs_processed.fetch_add(1);
}
catch (exception const& e) {
	ERROR_AS(cat, "Failed to import song \"{}\": {}", path, e.what());
	import_stats.songs_processed.fetch_add(1);
	import_stats.songs_failed.fetch_add(1);
}

inline auto Library::import_song(fs::path const& path) -> pair<isize, string>
{
	auto const is_archive = fs::is_regular_file(path);

	// First, determine if we're creating a new song or extending an existing song
	// We checksum all the charts and check if any of them already exist
	auto existing_song = optional<pair<isize, string>>{nullopt};
	auto get_song_from_chart = lib::sqlite::prepare(db, GetSongFromChartQuery);
	auto process_checksums = [&](MD5 md5) {
		lib::sqlite::query(get_song_from_chart, [&](isize id, string_view path) { existing_song.emplace(id, path); }, md5);
		if (existing_song) return false;
		return true;
	};
	if (is_archive)
		io::Song::for_each_chart_checksum_in_archive(path, process_checksums);
	else
		io::Song::for_each_chart_checksum_in_directory(path, process_checksums);

	if (existing_song) { // Extending
		INFO_AS(cat, "Song \"{}\" already exists in library; extending", path);
		auto existing_song_id = get<0>(*existing_song);
		auto const existing_song_path = fs::path{LibraryPath} / get<1>(*existing_song);

		auto tmp_path = existing_song_path;
		tmp_path.concat(".tmp");
		auto deleter = io::FileDeleter{tmp_path};
		if (is_archive)
			io::Song::extend_zip_from_archive(existing_song_path, path, tmp_path);
		else
			io::Song::extend_zip_from_directory(existing_song_path, path, tmp_path);

		fs::rename(tmp_path, existing_song_path);
		deleter.disarm();
		return {existing_song_id, existing_song_path.string()};
	} else { // New song
		auto out_filename = is_archive? path.stem().string() : path.filename().string();
		if (out_filename.empty())
			throw runtime_error_fmt("Failed to import \"{}\": invalid filename", path);
		out_filename = find_available_song_filename(out_filename);

		auto const out_path = fs::path{LibraryPath} / out_filename;
		auto deleter = io::FileDeleter{out_path};
		if (is_archive)
			io::Song::zip_from_archive(path, out_path);
		else
			io::Song::zip_from_directory(path, out_path);

		auto insert_song = lib::sqlite::prepare(db, InsertSongQuery);
		auto const song_id = lib::sqlite::insert(insert_song, out_filename);
		deleter.disarm();
		return {song_id, out_path.string()};
	}
}

inline auto Library::import_chart(io::Song& song, isize song_id, string chart_path, span<byte const> chart_raw) -> task<>
{
	if (stopping.load()) {
		WARN_AS(cat, "Chart import \"{}\" cancelled", chart_path);
		co_return;
	}

	auto chart_exists = lib::sqlite::prepare(db, ChartExistsQuery);
	auto const md5 = lib::openssl::md5(chart_raw);
	auto exists = false;
	lib::sqlite::query(chart_exists, [&] { exists = true; }, md5);
	if (exists) {
		INFO_AS(cat, "Chart import \"{}\" skipped (duplicate)", chart_path);
		import_stats.charts_skipped.fetch_add(1);
		co_return;
	}

	auto insert_chart = lib::sqlite::prepare(db, InsertChartQuery);
	auto insert_chart_density = lib::sqlite::prepare(db, InsertChartDensityQuery);
	auto insert_chart_import_log = lib::sqlite::prepare(db, InsertChartImportLogQuery);
	auto builder_cat = globals::logger->create_string_logger(lib::openssl::md5_to_hex(md5));
	INFO_AS(builder_cat, "Importing chart \"{}\"", chart_path);
	auto builder = Builder{builder_cat};
	auto chart = co_await builder.build(chart_raw, song);

	auto lock = co_await transaction_lock.scoped_lock();
    lib::sqlite::transaction(db, [&] {
        lib::sqlite::execute(insert_chart, chart->md5, song_id, chart_path, chart->metadata.title,
			chart->metadata.subtitle, chart->metadata.artist, chart->metadata.subartist,
			chart->metadata.genre, chart->metadata.url, chart->metadata.email,
			+chart->metadata.difficulty, +chart->metadata.playstyle, chart->metadata.features.has_ln,
			chart->metadata.features.has_soflan, chart->metadata.note_count,
			chart->metadata.chart_duration.count(), chart->metadata.audio_duration.count(),
			chart->metadata.loudness, chart->metadata.nps.average, chart->metadata.nps.peak,
			chart->metadata.bpm_range.min, chart->metadata.bpm_range.max,
			chart->metadata.bpm_range.main);

		auto serialize_density = [](vector<float> const& v) {
			auto [data, out] = lib::bits::data_out();
			out(v).or_throw();
			return data;
		};
		lib::sqlite::execute(insert_chart_density, chart->md5,
			chart->metadata.density.resolution.count(),
			serialize_density(chart->metadata.density.key),
			serialize_density(chart->metadata.density.scratch),
			serialize_density(chart->metadata.density.ln));
		lib::sqlite::execute(insert_chart_import_log, chart->md5, builder_cat.get_buffer());
    });
	dirty.store(true);
	import_stats.charts_added.fetch_add(1);
}

}

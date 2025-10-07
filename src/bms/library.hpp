/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/library.hpp:
A cache of song and chart metadata. Handles import events.
*/

#pragma once
#include "preamble.hpp"
#include "config.hpp"
#include "logger.hpp"
#include "lib/openssl.hpp"
#include "lib/sqlite.hpp"
#include "lib/bits.hpp"
#include "io/song.hpp"
#include "bms/builder.hpp"

namespace playnote::bms {

class Library {
public:
	// Minimal metadata about a chart in the library.
	struct ChartEntry {
		lib::openssl::MD5 md5;
		string title;
	};

	// Open an existing library, or create an empty one at the provided path.
	explicit Library(fs::path const&);

	// Import a song and all its charts into the library.
	void import(fs::path const&);

	// Return a list of all available charts.
	[[nodiscard]] auto list_charts() -> vector<ChartEntry>;

	// Load a chart from the library.
	auto load_chart(lib::openssl::MD5) -> shared_ptr<Chart const>;

	Library(Library const&) = delete;
	auto operator=(Library const&) -> Library& = delete;
	Library(Library&&) = delete;
	auto operator=(Library&&) -> Library& = delete;

private:
	// language=SQLite
	static constexpr auto SongsSchema = R"(
		CREATE TABLE IF NOT EXISTS songs(
			id INTEGER PRIMARY KEY,
			path TEXT NOT NULL UNIQUE
		)
	)"sv;
	// language=SQLite
	static constexpr auto SongExistsQuery = R"(
		SELECT 1 FROM songs WHERE path = ?1
	)"sv;
	// language=SQLite
	static constexpr auto InsertSongQuery = R"(
		INSERT INTO songs(path) VALUES(?1)
	)"sv;
	// language=SQLite
	static constexpr auto DeleteSongQuery = R"(
		DELETE FROM songs WHERE id = ?1
	)"sv;

	// language=SQLite
	static constexpr auto ChartsSchema = to_array({R"(
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
	)"sv, R"(
		CREATE INDEX IF NOT EXISTS charts_title ON charts(title)
	)"sv, R"(
		CREATE INDEX IF NOT EXISTS charts_subtitle ON charts(subtitle)
	)"sv, R"(
		CREATE INDEX IF NOT EXISTS charts_artist ON charts(artist)
	)"sv, R"(
		CREATE INDEX IF NOT EXISTS charts_subartist ON charts(subartist)
	)"sv, R"(
		CREATE INDEX IF NOT EXISTS charts_genre ON charts(genre)
	)"sv, R"(
		CREATE INDEX IF NOT EXISTS charts_difficulty ON charts(difficulty)
	)"sv, R"(
		CREATE INDEX IF NOT EXISTS charts_playstyle ON charts(playstyle)
	)"sv, R"(
		CREATE INDEX IF NOT EXISTS charts_note_count ON charts(note_count)
	)"sv, R"(
		CREATE INDEX IF NOT EXISTS charts_chart_duration ON charts(chart_duration)
	)"sv, R"(
		CREATE INDEX IF NOT EXISTS charts_average_nps ON charts(average_nps)
	)"sv, R"(
		CREATE INDEX IF NOT EXISTS charts_peak_nps ON charts(peak_nps)
	)"sv, R"(
		CREATE INDEX IF NOT EXISTS charts_main_bpm ON charts(main_bpm)
	)"sv});
	// language=SQLite
	static constexpr auto ChartExistsQuery = R"(
		SELECT 1 FROM charts WHERE md5 = ?1
	)"sv;
	// language=SQLite
	static constexpr auto ChartListingQuery = R"(
		SELECT md5, title, difficulty FROM charts
	)"sv;
	// language=SQLite
	static constexpr auto InsertChartQuery = R"(
		INSERT INTO charts(md5, song_id, path, title, subtitle, artist, subartist, genre, url,
			email, difficulty, playstyle, has_ln, has_soflan, note_count, chart_duration,
			audio_duration, loudness, average_nps, peak_nps, min_bpm, max_bpm, main_bpm)
			VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19, ?20, ?21, ?22, ?23)
	)"sv;

	// language=SQLite
	static constexpr auto ChartDensitiesSchema = to_array({R"(
		CREATE TABLE IF NOT EXISTS chart_densities(
			md5 BLOB UNIQUE NOT NULL REFERENCES charts ON DELETE CASCADE,
			resolution INTEGER NOT NULL CHECK(resolution >= 1),
			key BLOB NOT NULL,
			scratch BLOB NOT NULL,
			ln BLOB NOT NULL
		)
	)"sv, R"(
		CREATE INDEX IF NOT EXISTS chart_densities_md5 ON chart_densities(md5)
	)"sv});
	// language=SQLite
	static constexpr auto InsertChartDensityQuery = R"(
		INSERT INTO chart_densities(md5, resolution, key, scratch, ln) VALUES(?1, ?2, ?3, ?4, ?5)
	)"sv;

	// language=SQLite
	static constexpr auto GetSongFromChartQuery = R"(
		SELECT songs.id, songs.path FROM songs INNER JOIN charts ON songs.id = charts.song_id WHERE charts.md5 = ?1
	)"sv;
	// language=SQLite
	static constexpr auto SelectSongChartQuery = R"(
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
	)"sv;

	lib::sqlite::DB db;
	lib::sqlite::Statement song_exists;
	lib::sqlite::Statement insert_song;
	lib::sqlite::Statement delete_song;
	lib::sqlite::Statement chart_exists;
	lib::sqlite::Statement chart_listing;
	lib::sqlite::Statement insert_chart;
	lib::sqlite::Statement insert_chart_density;
	lib::sqlite::Statement get_song_from_chart;
	lib::sqlite::Statement select_song_chart;

	Builder builder;

	[[nodiscard]] auto find_available_song_filename(string_view name) -> string;
	void import_one(fs::path const&);
	auto import_song(fs::path const&) -> pair<usize, string>;
	auto import_chart(io::Song& song, usize song_id, string_view chart_path, span<byte const>) -> bool;
};

inline Library::Library(fs::path const& path):
	db{lib::sqlite::open(path)}
{
	lib::sqlite::execute(db, SongsSchema);
	lib::sqlite::execute(db, ChartsSchema);
	lib::sqlite::execute(db, ChartDensitiesSchema);
	song_exists = lib::sqlite::prepare(db, SongExistsQuery);
	insert_song = lib::sqlite::prepare(db, InsertSongQuery);
	delete_song = lib::sqlite::prepare(db, DeleteSongQuery);
	chart_exists = lib::sqlite::prepare(db, ChartExistsQuery);
	chart_listing = lib::sqlite::prepare(db, ChartListingQuery);
	insert_chart = lib::sqlite::prepare(db, InsertChartQuery);
	insert_chart_density = lib::sqlite::prepare(db, InsertChartDensityQuery);
	get_song_from_chart = lib::sqlite::prepare(db, GetSongFromChartQuery);
	select_song_chart = lib::sqlite::prepare(db, SelectSongChartQuery);
	fs::create_directory(LibraryPath);
	INFO("Opened song library at \"{}\"", path);
}

inline void Library::import(fs::path const& path)
{
	if (is_regular_file(path)) {
		import_one(path);
	} else if (is_directory(path)) {
		auto contents = vector<fs::directory_entry>{};
		copy(fs::directory_iterator{path}, back_inserter(contents));
		if (any_of(contents, [&](auto const& entry) { return fs::is_regular_file(entry) && io::Song::is_bms_ext(entry.path().extension().string()); }))
			import_one(path);
		else
			for (auto const& entry: contents) import(entry);
	} else {
		throw runtime_error_fmt("Failed to import \"{}\": unknown type of file", path);
	}
}

inline auto Library::list_charts() -> vector<ChartEntry>
{
	auto result = vector<ChartEntry>{};
	lib::sqlite::query(chart_listing, [&](span<byte const> md5, string_view title, int difficulty) {
		auto entry = ChartEntry{};
		copy(md5, entry.md5.begin());
		entry.title = format("{} [{}]", title, enum_name(static_cast<Difficulty>(difficulty)));
		result.emplace_back(move(entry));
	});
	return result;
}

inline auto Library::load_chart(lib::openssl::MD5 md5) -> shared_ptr<Chart const>
{
	auto cache = optional<Metadata>{nullopt};
	auto song_path = fs::path{};
	auto chart_path = string{};
	lib::sqlite::query(select_song_chart, [&](
		string_view song_path_sv, string_view chart_path_sv, int64 date_imported, string_view title, string_view subtitle,
		string_view artist, string_view subartist, string_view genre, string_view url, string_view email,
		int difficulty, int playstyle, int has_ln, int has_soflan, int note_count, int64 chart_duration,
		int64 audio_duration, double loudness, double average_nps, double peak_nps, double min_bpm, double max_bpm,
		double main_bpm, int density_resolution, span<const byte> density_key, span<const byte> density_scratch,
		span<const byte> density_ln
	) {
		auto deserialize_density = [](span<byte const> v) {
			auto [data, in] = lib::bits::data_in();
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
				.average = average_nps,
				.peak = peak_nps,
			},
			.bpm_range = Metadata::BPMRange{
				.min = min_bpm,
				.max = max_bpm,
				.main = main_bpm,
			},
		};
		return false;
	}, md5);
	if (!cache) throw runtime_error{"Chart not found"};

	auto song = io::Song::from_zip(song_path);
	auto chart_raw = song.load_file(chart_path);
	return builder.build(chart_raw, song, *cache);
}

inline auto Library::find_available_song_filename(string_view name) -> string
{
	for (auto i: views::iota(0u)) {
		auto test = i == 0?
			format("{}.zip", name) :
			format("{}-{}.zip", name, i);
		auto exists = false;
		lib::sqlite::query(song_exists, [&] { exists = true; }, test);
		if (!exists) return test;
	}
	unreachable();
}

inline void Library::import_one(fs::path const& path)
{
	auto [song_id, song_path] = import_song(path);
	auto song = io::Song::from_zip(song_path);
	auto imported_count = 0u;
	song.for_each_chart([&](auto path, auto chart) {
		imported_count += import_chart(song, song_id, path, chart)? 1 : 0;
	});
	if (imported_count == 0) {
		lib::sqlite::execute(delete_song, song_id);
		move(song).remove();
	}
}

inline auto Library::import_song(fs::path const& path) -> pair<usize, string> try
{
	auto const is_archive = fs::is_regular_file(path);

	// First, determine if we're creating a new song or extending an existing song
	// We checksum all the charts and check if any of them already exist
	auto existing_song = optional<pair<usize, string>>{nullopt};
	auto process_checksums = [&](lib::openssl::MD5 md5) {
		lib::sqlite::query(get_song_from_chart, [&](isize id, string_view path) { existing_song.emplace(id, path); }, md5);
		if (existing_song) return false;
		return true;
	};
	if (is_archive)
		io::Song::for_each_chart_checksum_in_archive(path, process_checksums);
	else
		io::Song::for_each_chart_checksum_in_directory(path, process_checksums);

	if (existing_song) { // Extending
		auto existing_song_id = get<0>(*existing_song);
		auto const existing_song_path = fs::path{LibraryPath} / get<1>(*existing_song);

		auto tmp_path = existing_song_path;
		tmp_path.concat(".tmp");
		if (is_archive)
			io::Song::extend_zip_from_archive(existing_song_path, path, tmp_path);
		else
			io::Song::extend_zip_from_directory(existing_song_path, path, tmp_path);

		fs::rename(tmp_path, existing_song_path);
		return {existing_song_id, existing_song_path.string()};
	} else { // New song
		auto out_filename = is_archive? path.stem().string() : path.filename().string();
		if (out_filename.empty())
			throw runtime_error_fmt("Failed to import \"{}\": invalid filename", path);
		out_filename = find_available_song_filename(out_filename);

		auto const out_path = fs::path{LibraryPath} / out_filename;
		if (is_archive)
			io::Song::zip_from_archive(path, out_path);
		else
			io::Song::zip_from_directory(path, out_path);

		auto const song_id = lib::sqlite::insert(insert_song, out_filename);
		return {song_id, out_path.string()};
	}
} catch (exception const&) {
	fs::remove(path);
	throw;
}

inline auto Library::import_chart(io::Song& song, usize song_id, string_view chart_path, span<byte const> chart_raw) -> bool
{
	auto const md5 = lib::openssl::md5(chart_raw);
	auto exists = false;
	lib::sqlite::query(chart_exists, [&] { exists = true; }, md5);
	if (exists) return false;

	auto chart = builder.build(chart_raw, song);
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
    });
	return true;
}

}

/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/library.hpp:
A database of chart information. Handles loading and saving of charts from/to the database.
*/

#pragma once
#include "preamble.hpp"
#include "config.hpp"
#include "logger.hpp"
#include "lib/sqlite.hpp"
#include "lib/bits.hpp"

namespace playnote::bms {

class Library {
public:
	// Open an existing library, or create an empty one at the provided path.
	explicit Library(fs::path const&);

	void import(fs::path const&);

	// Add a chart to the library. If it already exists, do nothing.
	void add_chart(fs::path const& domain, Chart const&);

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
	static constexpr auto InsertOrRetrieveSongQuery = R"(
		INSERT INTO songs(path) VALUES(?1) ON CONFLICT(path) DO UPDATE SET path=path RETURNING id
	)"sv;

	// language=SQLite
	static constexpr auto ChartsSchema = to_array({R"(
		CREATE TABLE IF NOT EXISTS charts(
			md5 BLOB PRIMARY KEY NOT NULL CHECK(length(md5) == 16),
			song_id INTEGER NOT NULL REFERENCES songs ON DELETE CASCADE,
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
	static constexpr auto InsertChartQuery = R"(
		INSERT INTO charts(md5, song_id, title, subtitle, artist, subartist, genre, url,
			email, difficulty, playstyle, has_ln, has_soflan, note_count, chart_duration,
			audio_duration, loudness, average_nps, peak_nps, min_bpm, max_bpm, main_bpm)
			VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19, ?20, ?21, ?22)
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
	static constexpr auto ChartIRsSchema = to_array({R"(
		CREATE TABLE IF NOT EXISTS chart_irs(
			md5 BLOB UNIQUE NOT NULL REFERENCES charts ON DELETE CASCADE,
			ir BLOB NOT NULL
		)
	)"sv, R"(
		CREATE INDEX IF NOT EXISTS chart_irs_md5 ON chart_irs(md5)
	)"sv});
	// language=SQLite
	static constexpr auto InsertChartIRQuery = R"(
		INSERT INTO chart_irs(md5, ir) VALUES(?1, ?2)
	)"sv;

	lib::sqlite::DB db;
	lib::sqlite::Statement song_insert_or_retrieve;
	lib::sqlite::Statement chart_exists;
	lib::sqlite::Statement chart_insert;
	lib::sqlite::Statement chart_density_insert;
	lib::sqlite::Statement chart_ir_insert;

	void import_song(fs::path const&);
};

inline Library::Library(fs::path const& path):
	db{lib::sqlite::open(path)}
{
	lib::sqlite::execute(db, SongsSchema);
	lib::sqlite::execute(db, ChartsSchema);
	lib::sqlite::execute(db, ChartDensitiesSchema);
	lib::sqlite::execute(db, ChartIRsSchema);
	song_insert_or_retrieve = lib::sqlite::prepare(db, InsertOrRetrieveSongQuery);
	chart_exists = lib::sqlite::prepare(db, ChartExistsQuery);
	chart_insert = lib::sqlite::prepare(db, InsertChartQuery);
	chart_density_insert = lib::sqlite::prepare(db, InsertChartDensityQuery);
	chart_ir_insert = lib::sqlite::prepare(db, InsertChartIRQuery);
	fs::create_directory(LibraryPath);
	INFO("Opened song library at \"{}\"", path);
}

inline void Library::import(fs::path const& path)
{
	static constexpr auto BMSExtensions = to_array({".bms", ".bme", ".bml", ".pms"});

	if (is_regular_file(path)) {
		import_song(path);
	} else if (is_directory(path)) {
		auto contents = vector<fs::directory_entry>{};
		copy(fs::directory_iterator{path}, back_inserter(contents));
		auto const contains_bms = any_of(contents, [&](auto const& entry) {
			return entry.is_regular_file() && contains(BMSExtensions, entry.path().extension());
		});
		if (contains_bms)
			import_song(path);
		else
			for (auto const& entry: contents) import(entry);
	} else {
		throw runtime_error_fmt("Failed to import \"{}\": unknown type of file", path);
	}
}

inline void Library::add_chart(fs::path const& domain, Chart const& chart)
{
	lib::sqlite::transaction(db, [&] {
		// Check if chart already exists in library
		auto exists = false;
		lib::sqlite::query(chart_exists, [&]() { exists = true; }, chart.md5);
		if (exists) return;

		static constexpr auto BlobPlaceholder = to_array<unsigned char const>({0x01, 0x02, 0x03, 0x04});
		auto song_id = 0;
		lib::sqlite::query(song_insert_or_retrieve, [&](int id) { song_id = id; }, domain.string());
		lib::sqlite::execute(chart_insert, chart.md5, song_id, chart.metadata.title,
			chart.metadata.subtitle, chart.metadata.artist, chart.metadata.subartist,
			chart.metadata.genre, chart.metadata.url, chart.metadata.email,
			+chart.metadata.difficulty, +chart.timeline.playstyle, chart.metadata.features.has_ln,
			chart.metadata.features.has_soflan, chart.metadata.note_count,
			chart.metadata.chart_duration.count(), chart.metadata.audio_duration.count(),
			chart.metadata.loudness, chart.metadata.nps.average, chart.metadata.nps.peak,
			chart.metadata.bpm_range.min, chart.metadata.bpm_range.max,
			chart.metadata.bpm_range.main);

		auto serialize_density = [](vector<float> const& v) {
			auto [data, out] = lib::bits::data_out();
			out(v).or_throw();
			return data;
		};
		lib::sqlite::execute(chart_density_insert, chart.md5,
			chart.metadata.density.resolution.count(),
			serialize_density(chart.metadata.density.key),
			serialize_density(chart.metadata.density.scratch),
			serialize_density(chart.metadata.density.ln));
		lib::sqlite::execute(chart_ir_insert, chart.md5, BlobPlaceholder);
	});
}

inline void Library::import_song(fs::path const& path)
{
	INFO("Importing song at \"{}\"", path);
}

}

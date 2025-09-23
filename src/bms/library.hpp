/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/library.hpp:
A database of chart information. Holds cached IR blobs and calculated metadata.
*/

#pragma once
#include "preamble.hpp"
#include "logger.hpp"
#include "lib/sqlite.hpp"

namespace playnote::bms {

class Library {
public:
	// Open an existing library, or create an empty one at the provided path.
	explicit Library(fs::path const&);
	~Library() noexcept;

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
	static constexpr auto ChartsSchema = to_array({R"(
		CREATE TABLE IF NOT EXISTS charts(
			md5 BLOB PRIMARY KEY CHECK(length(md5) == 16),
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
			initial_bpm REAL NOT NULL,
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
		CREATE INDEX IF NOT EXISTS charts_chart_duration ON charts(chart_duration)
	)"sv, R"(
		CREATE INDEX IF NOT EXISTS charts_artist ON charts(artist)
	)"sv, R"(
		CREATE INDEX IF NOT EXISTS charts_average_nps ON charts(average_nps)
	)"sv, R"(
		CREATE INDEX IF NOT EXISTS charts_peak_nps ON charts(peak_nps)
	)"sv, R"(
		CREATE INDEX IF NOT EXISTS charts_main_bpm ON charts(main_bpm)
	)"sv});

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
	static constexpr auto ChartIRsSchema = to_array({R"(
		CREATE TABLE IF NOT EXISTS chart_irs(
			md5 BLOB UNIQUE NOT NULL REFERENCES charts ON DELETE CASCADE,
			ir BLOB NOT NULL
		)
	)"sv, R"(
		CREATE INDEX IF NOT EXISTS chart_irs_md5 ON chart_irs(md5)
	)"sv});

	lib::sqlite::DB db;
};

inline Library::Library(fs::path const& path):
	db{lib::sqlite::open(path)}
{
	lib::sqlite::execute(db, SongsSchema);
	lib::sqlite::execute(db, ChartsSchema);
	lib::sqlite::execute(db, ChartDensitiesSchema);
	lib::sqlite::execute(db, ChartIRsSchema);
	INFO("Opened song library at \"{}\"", path);
}

inline Library::~Library() noexcept
{
	lib::sqlite::close(db);
}

}

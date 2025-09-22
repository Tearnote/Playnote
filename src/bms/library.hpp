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
			title TEXT NOT NULL
		)
	)"sv, R"(
		CREATE INDEX IF NOT EXISTS charts_title ON charts(title)
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
	lib::sqlite::execute(db, ChartIRsSchema);
	INFO("Opened song library at \"{}\"", path);
}

inline Library::~Library() noexcept
{
	lib::sqlite::close(db);
}

}

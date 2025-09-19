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

	// Register chart in the library, if not already present.
	void add_chart(Chart const&);

	Library(Library const&) = delete;
	auto operator=(Library const&) -> Library& = delete;
	Library(Library&&) = delete;
	auto operator=(Library&&) -> Library& = delete;

private:
	// language=SQLite
	static constexpr auto ChartsSchema = R"(
		CREATE TABLE IF NOT EXISTS charts(
			md5 BLOB PRIMARY KEY CHECK(length(md5) == 16),
			date_imported INTEGER DEFAULT(unixepoch()),
			title TEXT NOT NULL
		)
	)"sv;

	// language=SQLite
	static constexpr auto ChartInsertQuery = R"(
		INSERT OR IGNORE INTO charts(md5, title) VALUES(?1, ?2)
	)"sv;

	lib::sqlite::DB db;
	lib::sqlite::Statement insert_chart;
};

inline Library::Library(fs::path const& path):
	db{lib::sqlite::open(path)}
{
	lib::sqlite::execute(db, ChartsSchema);
	insert_chart = lib::sqlite::create_statement(db, ChartInsertQuery);
	INFO("Opened song library at \"{}\"", path);
}

inline Library::~Library() noexcept
{
	lib::sqlite::destroy_statement(insert_chart);
	lib::sqlite::close(db);
}

inline void Library::add_chart(Chart const& chart)
{
	lib::sqlite::execute(insert_chart, chart.md5, chart.metadata.title);
}

}

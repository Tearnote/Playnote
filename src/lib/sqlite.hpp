/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/sqlite.hpp:
Wrapper for sqlite database operations.
*/

#pragma once
#include <sqlite3.h>
#include "preamble.hpp"
#include "assert.hpp"

// Forward declarations

struct sqlite3;

namespace playnote::lib::sqlite {

using DB = sqlite3*;

// Helper functions for error handling

static auto ret_check(int ret) -> int
{
	if (ret != SQLITE_OK) throw runtime_error_fmt("sqlite error: {}", sqlite3_errstr(ret));
	return ret;
}

// Open an existing database, or create a new one if it doesn't exist yet. The database
// is guaranteed to be read-write.
// Throws runtime_error on sqlite error, or if database could only be opened read-only.
auto open(fs::path const&) -> DB;

// Close a previously opened database. Execute this to free any allocated resources.
void close(DB) noexcept;

// Run a SQL query on the database. Use only for one-time queries that don't return data.
// Throws runtime_error on sqlite error.
void execute(DB, string_view query);

inline auto open(fs::path const& path) -> DB
{
	auto db = static_cast<DB>(nullptr);
	auto check = [&](int ret) {
		if (ret != SQLITE_OK) {
			sqlite3_close(db);
			ret_check(ret);
		}
	};
	check(sqlite3_open(path.string().c_str(), &db));

	ASSUME(db);
	check(sqlite3_extended_result_codes(db, true));
	if (sqlite3_db_readonly(db, "main") == 1) {
		sqlite3_close(db);
		throw runtime_error{"Database is read-only"};
	}
	return db;
}

inline void close(DB db) noexcept
{
	sqlite3_close(db);
}

inline void execute(DB db, string_view query)
{
	auto* stmt = static_cast<sqlite3_stmt*>(nullptr);
	ret_check(sqlite3_prepare_v2(db, query.data(), query.size(), &stmt, nullptr));
	auto check = [&](int ret) {
		if (ret != SQLITE_OK) {
			sqlite3_finalize(stmt);
			ret_check(ret);
		}
	};
	auto ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE && ret != SQLITE_ROW) check(ret);
	sqlite3_finalize(stmt);
}

}

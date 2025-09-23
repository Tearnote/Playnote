/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/sqlite.cpp:
Implementation file for lib/sqlite.hpp.
*/

#include "lib/sqlite.hpp"
#include "assert.hpp"

#include <sqlite3.h>

namespace playnote::lib::sqlite {

// Helper functions for error handling

static auto ret_check(int ret) -> int
{
	if (ret != SQLITE_OK) throw runtime_error_fmt("sqlite error: {}", sqlite3_errstr(ret));
	return ret;
}

static auto ret_check_ext(DB db, int ret) -> int
{
	if (ret != SQLITE_OK) throw runtime_error_fmt("sqlite error: {}", sqlite3_errmsg(db));
	return ret;
}

auto open(fs::path const& path) -> DB
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
	execute(db, "PRAGMA foreign_keys = ON");
	execute(db, "PRAGMA journal_mode = WAL");
	execute(db, "PRAGMA trusted_schema = OFF");
	execute(db, "PRAGMA mmap_size = 268435456"); // 256 MB
	return db;
}

void close(DB db) noexcept
{
	sqlite3_close(db);
}

void execute(DB db, string_view statement)
{
	auto stmt = prepare(db, statement);
	auto check = [&](int ret) {
		if (ret != SQLITE_OK) {
			sqlite3_finalize(stmt);
			ret_check_ext(db, ret);
		}
	};

	auto const ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE && ret != SQLITE_ROW) check(ret);

	finalize(stmt);
}

void execute(DB db, span<string_view const> statements)
{
	for (auto statement: statements) execute(db, statement);
}

auto prepare(DB db, string_view query) -> Statement
{
	auto stmt = Statement{};
	ret_check_ext(db, sqlite3_prepare_v2(db, query.data(), query.size(), &stmt, nullptr));
	return stmt;
}

void finalize(Statement stmt) noexcept
{
	sqlite3_finalize(stmt);
}

void detail::bind_int(Statement stmt, int idx, int arg)
{
	ret_check_ext(sqlite3_db_handle(stmt), sqlite3_bind_int(stmt, idx, arg));
}

void detail::bind_int64(Statement stmt, int idx, int64 arg)
{
	ret_check_ext(sqlite3_db_handle(stmt), sqlite3_bind_int64(stmt, idx, arg));
}

void detail::bind_double(Statement stmt, int idx, double arg)
{
	ret_check_ext(sqlite3_db_handle(stmt), sqlite3_bind_double(stmt, idx, arg));
}

void detail::bind_text(Statement stmt, int idx, string_view arg)
{
	ret_check_ext(sqlite3_db_handle(stmt), sqlite3_bind_text(stmt, idx, arg.data(), arg.size(), SQLITE_TRANSIENT));
}

void detail::bind_blob(Statement stmt, int idx, span<byte const> arg)
{
	ret_check_ext(sqlite3_db_handle(stmt), sqlite3_bind_blob(stmt, idx, arg.data(), arg.size(), SQLITE_TRANSIENT));
}

void detail::execute(Statement stmt)
{
	auto const ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE && ret != SQLITE_ROW) {
		ret_check_ext(sqlite3_db_handle(stmt), sqlite3_reset(stmt));
		ret_check_ext(sqlite3_db_handle(stmt), ret);
	}
	ret_check_ext(sqlite3_db_handle(stmt), sqlite3_reset(stmt));
}

void detail::begin_transaction(DB db)
{
	sqlite::execute(db, "BEGIN TRANSACTION");
}

void detail::end_transaction(DB db)
{
	sqlite::execute(db, "END TRANSACTION");
}

auto detail::last_insert_rowid(Statement stmt) -> int64
{
	return sqlite3_last_insert_rowid(sqlite3_db_handle(stmt));
}

}

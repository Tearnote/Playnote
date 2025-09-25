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

auto detail::step(Statement stmt) -> QueryStatus
{
	auto const ret = sqlite3_step(stmt);
	if (ret == SQLITE_DONE || ret == SQLITE_ROW)
		return static_cast<QueryStatus>(ret);
	reset(stmt);
	ret_check_ext(sqlite3_db_handle(stmt), ret);
	unreachable(); // The line above is guaranteed to throw
}

void detail::reset(Statement stmt)
{
	ret_check_ext(sqlite3_db_handle(stmt), sqlite3_reset(stmt));
}

void detail::begin_transaction(DB db)
{
	execute(db, "BEGIN TRANSACTION");
}

void detail::end_transaction(DB db)
{
	execute(db, "END TRANSACTION");
}

auto detail::last_insert_rowid(Statement stmt) -> int64
{
	return sqlite3_last_insert_rowid(sqlite3_db_handle(stmt));
}

template<>
auto detail::get_column<int>(Statement stmt, int idx) -> int
{
	return sqlite3_column_int(stmt, idx);
}

template<>
auto detail::get_column<int64>(Statement stmt, int idx) -> int64
{
	return sqlite3_column_int64(stmt, idx);
}

template<>
auto detail::get_column<double>(Statement stmt, int idx) -> double
{
	return sqlite3_column_double(stmt, idx);
}

template<>
auto detail::get_column<string_view>(Statement stmt, int idx) -> string_view
{
	auto const* text = reinterpret_cast<char const*>(sqlite3_column_text(stmt, idx));
	auto const len = sqlite3_column_bytes(stmt, idx);
	// The string pointed at by string_view lives until the next step() or reset(),
	// which is guaranteed by the usage of this function in query()
	return {text, static_cast<usize>(len)};
}

template<>
auto detail::get_column<span<byte const>>(Statement stmt, int idx) -> span<byte const>
{
	auto const* blob = reinterpret_cast<byte const*>(sqlite3_column_blob(stmt, idx));
	auto const size = sqlite3_column_bytes(stmt, idx);
	// The buffer pointed at by the span lives until the next step() or reset(),
	// which is guaranteed by the usage of this function in query()
	return {blob, static_cast<usize>(size)};
}

}

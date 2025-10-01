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

static auto ret_check_ext(sqlite3* db_raw, int ret) -> int
{
	if (ret != SQLITE_OK) throw runtime_error_fmt("sqlite error: {}", sqlite3_errmsg(db_raw));
	return ret;
}

auto open(fs::path const& path) -> DB
{
	auto db_raw = static_cast<sqlite3*>(nullptr);
	auto check = [&](int ret) {
		if (ret != SQLITE_OK) {
			sqlite3_close(db_raw);
			ret_check(ret);
		}
	};
	check(sqlite3_open(path.string().c_str(), &db_raw));
	ASSUME(db_raw);

	auto db = DB{db_raw};
	ret_check_ext(db.get(), sqlite3_extended_result_codes(db.get(), true));
	if (sqlite3_db_readonly(db.get(), "main") == 1)
		throw runtime_error{"Database is read-only"};
	execute(db, "PRAGMA foreign_keys = ON");
	execute(db, "PRAGMA journal_mode = WAL");
	execute(db, "PRAGMA trusted_schema = OFF");
	execute(db, "PRAGMA mmap_size = 268435456"); // 256 MB
	return db;
}

auto prepare(DB& db, string_view query) -> Statement
{
	auto stmt_raw = static_cast<sqlite3_stmt*>(nullptr);
	ret_check_ext(db.get(), sqlite3_prepare_v2(db.get(), query.data(), query.size(), &stmt_raw, nullptr));
	ASSUME(stmt_raw);
	return Statement{stmt_raw};
}

void execute(DB& db, string_view query_str)
{
	auto stmt = prepare(db, query_str);
	query(stmt, []{});
}

void execute(DB& db, span<string_view const> queries)
{
	for (auto query: queries) execute(db, query);
}

void detail::DBDeleter::operator()(sqlite3* db) noexcept
{
	sqlite3_close(db);
}

void detail::StatementDeleter::operator()(sqlite3_stmt* stmt) noexcept
{
	sqlite3_finalize(stmt);
}

template<>
void detail::bind<int>(Statement& stmt, int idx, int arg)
{
	ret_check_ext(sqlite3_db_handle(stmt.get()), sqlite3_bind_int(stmt.get(), idx, arg));
}

template<>
void detail::bind<int64>(Statement& stmt, int idx, int64 arg)
{
	ret_check_ext(sqlite3_db_handle(stmt.get()), sqlite3_bind_int64(stmt.get(), idx, arg));
}

template<>
void detail::bind<double>(Statement& stmt, int idx, double arg)
{
	ret_check_ext(sqlite3_db_handle(stmt.get()), sqlite3_bind_double(stmt.get(), idx, arg));
}

template<>
void detail::bind<string_view>(Statement& stmt, int idx, string_view arg)
{
	ret_check_ext(sqlite3_db_handle(stmt.get()), sqlite3_bind_text(stmt.get(), idx, arg.data(), arg.size(), SQLITE_TRANSIENT));
}

template<>
void detail::bind<span<byte const>>(Statement& stmt, int idx, span<byte const> arg)
{
	ret_check_ext(sqlite3_db_handle(stmt.get()), sqlite3_bind_blob(stmt.get(), idx, arg.data(), arg.size(), SQLITE_TRANSIENT));
}

template<>
void detail::bind<void const*>(Statement& stmt, int idx, void const* arg)
{
	ret_check_ext(sqlite3_db_handle(stmt.get()), sqlite3_bind_blob(stmt.get(), idx, &arg, sizeof(void const*), SQLITE_TRANSIENT));
}

auto detail::step(Statement& stmt) -> QueryStatus
{
	auto const ret = sqlite3_step(stmt.get());
	if (ret == SQLITE_DONE || ret == SQLITE_ROW)
		return static_cast<QueryStatus>(ret);
	reset(stmt);
	ret_check_ext(sqlite3_db_handle(stmt.get()), ret);
	unreachable(); // The line above is guaranteed to throw
}

void detail::reset(Statement& stmt)
{
	ret_check_ext(sqlite3_db_handle(stmt.get()), sqlite3_reset(stmt.get()));
}

void detail::begin_transaction(DB& db)
{
	execute(db, "BEGIN TRANSACTION");
}

void detail::end_transaction(DB& db)
{
	execute(db, "END TRANSACTION");
}

auto detail::last_insert_rowid(Statement& stmt) -> int64
{
	return sqlite3_last_insert_rowid(sqlite3_db_handle(stmt.get()));
}

template<>
auto detail::get_column<int>(Statement& stmt, int idx) -> int
{
	return sqlite3_column_int(stmt.get(), idx);
}

template<>
auto detail::get_column<int64>(Statement& stmt, int idx) -> int64
{
	return sqlite3_column_int64(stmt.get(), idx);
}

template<>
auto detail::get_column<double>(Statement& stmt, int idx) -> double
{
	return sqlite3_column_double(stmt.get(), idx);
}

template<>
auto detail::get_column<string_view>(Statement& stmt, int idx) -> string_view
{
	auto const* text = reinterpret_cast<char const*>(sqlite3_column_text(stmt.get(), idx));
	auto const len = sqlite3_column_bytes(stmt.get(), idx);
	// The string pointed at by string_view lives until the next step() or reset(),
	// which is guaranteed by the usage of this function in query()
	return {text, static_cast<usize>(len)};
}

template<>
auto detail::get_column<span<byte const>>(Statement& stmt, int idx) -> span<byte const>
{
	auto const* blob = reinterpret_cast<byte const*>(sqlite3_column_blob(stmt.get(), idx));
	auto const size = sqlite3_column_bytes(stmt.get(), idx);
	// The buffer pointed at by the span lives until the next step() or reset(),
	// which is guaranteed by the usage of this function in query()
	return {blob, static_cast<usize>(size)};
}

template<>
auto detail::get_column<void const*>(Statement& stmt, int idx) -> void const*
{
	auto* ptr = static_cast<void const* const*>(sqlite3_column_blob(stmt.get(), idx));
	return *ptr;
}

}

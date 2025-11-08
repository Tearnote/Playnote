/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "lib/sqlite.hpp"

#include <sqlite3.h>
#include "preamble.hpp"
#include "utils/assert.hpp"

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
	if (sqlite3_threadsafe() == 0)
		throw runtime_error{"sqlite was built without thread-safe support"};
	auto db_raw = static_cast<sqlite3*>(nullptr);
	auto check = [&](int ret) {
		if (ret != SQLITE_OK) {
			sqlite3_close(db_raw);
			ret_check(ret);
		}
	};
	check(sqlite3_open_v2(path.string().c_str(), &db_raw,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr));
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

void detail::MutexDeleter::operator()(sqlite3_mutex* m) noexcept
{
	sqlite3_mutex_leave(m);
}

template<>
void detail::bind<int>(Statement& stmt, int idx, int arg)
{
	ret_check_ext(sqlite3_db_handle(stmt.get()), sqlite3_bind_int(stmt.get(), idx, arg));
}

template<>
void detail::bind<int64_t>(Statement& stmt, int idx, int64_t arg)
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

detail::ScopedTransaction::ScopedTransaction(DB& db):
	db{db}
{
	execute(db, "BEGIN TRANSACTION");
}

detail::ScopedTransaction::~ScopedTransaction()
{
	if (!committed) execute(db, "ROLLBACK");
}

void detail::ScopedTransaction::commit()
{
	execute(db, "END TRANSACTION");
	committed = true;
}

auto detail::last_insert_rowid(Statement& stmt) -> int64_t
{
	return sqlite3_last_insert_rowid(sqlite3_db_handle(stmt.get()));
}

auto detail::acquire_db_mutex(DB& db) -> Mutex
{
	auto raw_mutex = sqlite3_db_mutex(db.get());
	sqlite3_mutex_enter(raw_mutex);
	return Mutex{raw_mutex};
}

template<>
auto detail::get_column<int>(Statement& stmt, int idx) -> int
{
	return sqlite3_column_int(stmt.get(), idx);
}

template<>
auto detail::get_column<int64_t>(Statement& stmt, int idx) -> int64_t
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
	return {text, static_cast<size_t>(len)};
}

template<>
auto detail::get_column<span<byte const>>(Statement& stmt, int idx) -> span<byte const>
{
	auto const* blob = reinterpret_cast<byte const*>(sqlite3_column_blob(stmt.get(), idx));
	auto const size = sqlite3_column_bytes(stmt.get(), idx);
	// The buffer pointed at by the span lives until the next step() or reset(),
	// which is guaranteed by the usage of this function in query()
	return {blob, static_cast<size_t>(size)};
}

template<>
auto detail::get_column<void const*>(Statement& stmt, int idx) -> void const*
{
	auto* ptr = static_cast<void const* const*>(sqlite3_column_blob(stmt.get(), idx));
	return *ptr;
}

}

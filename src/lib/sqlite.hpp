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
struct sqlite3_stmt;

namespace playnote::lib::sqlite {

using DB = sqlite3*;
using Statement = sqlite3_stmt*;

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

// Compile a query into a statement object. Can contain numbered placeholders.
// Throws runtime_error on sqlite error.
auto create_statement(DB, string_view query) -> Statement;

// Destroy a statement, freeing related resources.
void destroy_statement(Statement) noexcept;

// Execute a statement with provided parameters, discarding any output data.
template<typename... Args>
void execute(Statement, Args&&... args);

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
	auto stmt = create_statement(db, query);
	auto check = [&](int ret) {
		if (ret != SQLITE_OK) {
			sqlite3_finalize(stmt);
			ret_check(ret);
		}
	};

	auto const ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE && ret != SQLITE_ROW) check(ret);

	destroy_statement(stmt);
}

inline auto create_statement(DB db, string_view query) -> Statement
{
	auto stmt = Statement{};
	ret_check(sqlite3_prepare_v2(db, query.data(), query.size(), &stmt, nullptr));
	return stmt;
}

inline void destroy_statement(Statement stmt) noexcept
{
	sqlite3_finalize(stmt);
}

template<typename ... Args>
void execute(Statement stmt, Args&&... args)
{
	auto bind = [&](int idx, auto&& arg) {
		using ArgT = remove_cvref_t<decltype(arg)>;
		if constexpr (same_as<ArgT, float> || same_as<ArgT, double>)
			ret_check(sqlite3_bind_double(stmt, idx, arg));
		else if constexpr (convertible_to<ArgT, int> || same_as<ArgT, bool>)
			ret_check(sqlite3_bind_int(stmt, idx, arg));
		else if constexpr (convertible_to<ArgT, sqlite3_int64>)
			ret_check(sqlite3_bind_int64(stmt, idx, arg));
		else if constexpr (same_as<ArgT, string> || same_as<ArgT, string_view>)
			ret_check(sqlite3_bind_text(stmt, idx, arg.data(), arg.size(), SQLITE_TRANSIENT));
		else if constexpr (convertible_to<ArgT, span<byte const>>)
			ret_check(sqlite3_bind_blob(stmt, idx, arg.data(), arg.size(), SQLITE_TRANSIENT));
	};

	auto index = 1;
	(bind(index++, forward<Args>(args)), ...);

	auto const ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE && ret != SQLITE_ROW) {
		ret_check(sqlite3_reset(stmt));
		ret_check(ret);
	}
	ret_check(sqlite3_reset(stmt));
}

}

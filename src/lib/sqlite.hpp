/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/sqlite.hpp:
Wrapper for sqlite database operations.
*/

#pragma once
#include "preamble.hpp"

// Forward declarations

struct sqlite3;
struct sqlite3_stmt;

namespace playnote::lib::sqlite {

using DB = sqlite3*;
using Statement = sqlite3_stmt*;

// Open an existing database, or create a new one if it doesn't exist yet. The database
// is guaranteed to be read-write.
// Throws runtime_error on sqlite error, or if database could only be opened read-only.
auto open(fs::path const&) -> DB;

// Close a previously opened database. Execute this to free any allocated resources.
void close(DB) noexcept;

// Execute a single SQL statement on the database, discarding any output.
// Throws runtime_error on sqlite error.
void execute(DB, string_view statement);

// Execute a list of SQL statements on the database, discarding any output. This must be used for
// executions that contain multiple statements, as the single-statement version doesn't support
// splitting by the ";" character.
// Throws runtime_error on sqlite error.
void execute(DB, span<string_view const> statements);

// Compile a query into a statement object. Can contain numbered placeholders.
// Throws runtime_error on sqlite error.
auto create_statement(DB, string_view query) -> Statement;

// Destroy a statement, freeing related resources.
void destroy_statement(Statement) noexcept;

// Execute a statement with provided parameters, discarding any output data.
template<typename... Args>
void execute(Statement, Args&&... args);

namespace detail {
	void bind_int(Statement, int idx, int arg);
	void bind_int64(Statement, int idx, int64 arg);
	void bind_double(Statement, int idx, double arg);
	void bind_text(Statement, int idx, string_view arg);
	void bind_blob(Statement, int idx, span<byte const> arg);
	void execute(Statement);
}

template<typename ... Args>
void execute(Statement stmt, Args&&... args)
{
	auto bind = [&](int idx, auto&& arg) {
		using ArgT = remove_cvref_t<decltype(arg)>;
		if constexpr (same_as<ArgT, float> || same_as<ArgT, double>)
			detail::bind_double(stmt, idx, arg);
		else if constexpr (convertible_to<ArgT, int> || same_as<ArgT, bool>)
			detail::bind_int(stmt, idx, arg);
		else if constexpr (convertible_to<ArgT, int64>)
			detail::bind_int64(stmt, idx, arg);
		else if constexpr (same_as<ArgT, string> || same_as<ArgT, string_view>)
			detail::bind_text(stmt, idx, arg);
		else if constexpr (convertible_to<ArgT, span<byte const>>)
			detail::bind_blob(stmt, idx, arg);
	};
	auto index = 1;
	(bind(index++, forward<Args>(args)), ...);
	detail::execute(stmt);
}

}

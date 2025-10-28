/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"

// Forward declarations

struct sqlite3;
struct sqlite3_stmt;
struct sqlite3_mutex;

namespace playnote::lib::sqlite {

namespace detail {
struct DBDeleter {
	static void operator()(sqlite3* db) noexcept;
};
struct StatementDeleter {
	static void operator()(sqlite3_stmt* st) noexcept;
};
struct MutexDeleter {
	static void operator()(sqlite3_mutex* m) noexcept;
};
using Mutex = unique_resource<sqlite3_mutex*, MutexDeleter>;
}

using DB = unique_resource<sqlite3*, detail::DBDeleter>;
using Statement = unique_resource<sqlite3_stmt*, detail::StatementDeleter>;

namespace detail {
enum class QueryStatus: int {
	Done = 101, // SQLITE_DONE
	Row = 100, // SQLITE_ROW
};

template<typename T>
           void bind                  (Statement&, int idx, T arg);
template<> void bind<int>             (Statement&, int idx, int arg);
template<> void bind<int64>           (Statement&, int idx, int64 arg);
template<> void bind<double>          (Statement&, int idx, double arg);
template<> void bind<string_view>     (Statement&, int idx, string_view arg);
template<> void bind<span<byte const>>(Statement&, int idx, span<byte const> arg);
template<> void bind<void const*>     (Statement&, int idx, void const* arg);

auto step(Statement&) -> QueryStatus;
void reset(Statement&);
auto last_insert_rowid(Statement&) -> int64;
auto acquire_db_mutex(DB&) -> detail::Mutex;

template<typename T>
           auto get_column                  (Statement&, int idx) -> T;
template<> auto get_column<int>             (Statement&, int idx) -> int;
template<> auto get_column<int64>           (Statement&, int idx) -> int64;
template<> auto get_column<double>          (Statement&, int idx) -> double;
template<> auto get_column<string_view>     (Statement&, int idx) -> string_view;
template<> auto get_column<span<byte const>>(Statement&, int idx) -> span<byte const>;
template<> auto get_column<void const*>     (Statement&, int idx) -> void const*;

class ScopedTransaction {
public:
	explicit ScopedTransaction(DB& db);
	~ScopedTransaction();
	void commit();

	ScopedTransaction(ScopedTransaction const&) = delete;
	auto operator=(ScopedTransaction const&) -> ScopedTransaction& = delete;
	ScopedTransaction(ScopedTransaction&&) = delete;
	auto operator=(ScopedTransaction&&) -> ScopedTransaction& = delete;

private:
	DB& db;
	bool committed = false;
};
}

// Open an existing database, or create a new one if it doesn't exist yet. The database
// is guaranteed to be read-write.
// Throws runtime_error on sqlite error, or if database could only be opened read-only.
auto open(fs::path const&) -> DB;

// Compile a query into a statement object. Can contain numbered placeholders.
// Throws runtime_error on sqlite error.
auto prepare(DB&, string_view query) -> Statement;

// Execute a single SQL query on the database, discarding any output.
// Throws runtime_error on sqlite error.
void execute(DB&, string_view query);

// Execute a list of SQL queries on the database, discarding any output. This must be used for
// executions that contain multiple statements, as the single-statement version doesn't support
// splitting by the ";" character.
// Throws runtime_error on sqlite error.
void execute(DB&, span<string_view const> query);

// Execute a statement with provided parameters, discarding any output data.
// Throws runtime_error on sqlite error.
template<typename... Args>
void execute(Statement&, Args&&... args);

// Execute an insert statement with provided parameters, and return the rowid of the inserted row.
// Throws runtime_error on sqlite error.
template<typename... Args>
auto insert(Statement&, Args&&... args) -> int64;

// Execute a statement with provided parameters, and call the provided function on every resulting row.
// Throws runtime_error on sqlite error.
template<typename Func, typename... Args>
void query(Statement&, Func&&, Args&&... args);

// Bundle queries into an atomic transaction. All queries executed within the provided function
// will be executed wholly or not at all.
// Throws runtime_error on sqlite error.
template<callable<void()> Func>
void transaction(DB&, Func&&);

template<typename ... Args>
void execute(Statement& stmt, Args&&... args) { query(stmt, []{}, forward<Args>(args)...); }

template<typename ... Args>
auto insert(Statement& stmt, Args&&... args) -> int64
{
	query(stmt, []{}, forward<Args>(args)...);
	return detail::last_insert_rowid(stmt);
}

template<typename Func, typename... Args>
void query(Statement& stmt, Func&& func, Args&&... args)
{
	// Bind statement parameters
	[[maybe_unused]] auto bind = [&](int idx, auto&& arg) {
		using ArgT = remove_cvref_t<decltype(arg)>;
		if constexpr (same_as<ArgT, float> || same_as<ArgT, double>)
			detail::bind<double>(stmt, idx, arg);
		else if constexpr (same_as<ArgT, int64> || same_as<ArgT, uint64>)
			detail::bind<int64>(stmt, idx, arg);
		else if constexpr (convertible_to<ArgT, int> || same_as<ArgT, bool>)
			detail::bind<int>(stmt, idx, arg);
		else if constexpr (same_as<ArgT, char const*> || same_as<ArgT, string> || same_as<ArgT, string_view>)
			detail::bind<string_view>(stmt, idx, arg);
		else if constexpr (convertible_to<ArgT, span<byte const>> || convertible_to<ArgT, span<unsigned char const>>)
			detail::bind<span<byte const>>(stmt, idx, {reinterpret_cast<byte const*>(arg.data()), arg.size()});
		else if constexpr (same_as<ArgT, void*> || same_as<ArgT, void const*>)
			detail::bind<void const*>(stmt, idx, arg);
		else
			static_assert(false, "Unknown sqlite binding type");
	};
	auto index = 1;
	(bind(index++, forward<Args>(args)), ...);

	// Step through every result row
	using FuncParams = function_traits<Func>::params;
	while (detail::step(stmt) == detail::QueryStatus::Row) {
		auto const process_row = [&]<usize... I>(index_sequence<I...>) {
			auto row = make_tuple(detail::get_column<tuple_element_t<I, FuncParams>>(stmt, I)...);
			apply(func, row);
		};
		process_row(make_index_sequence<tuple_size_v<FuncParams>>{});
	}

	// Finished; reset the statement
	detail::reset(stmt);
}

template<callable<void()> Func>
void transaction(DB& db, Func&& func)
{
	auto lock = detail::acquire_db_mutex(db);
	auto tx = detail::ScopedTransaction(db);
	func();
	tx.commit();
}

}

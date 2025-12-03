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

template<typename T, typename = void>
struct get_params { using type = tuple<>; };
template<typename T>
struct get_params<T, void_t<typename T::Params>> {
	using type = typename T::Params;
};
template<typename T>
using params_t = typename get_params<T>::type;

template<typename T, typename = void>
struct get_row { using type = tuple<>; };
template<typename T>
struct get_row<T, void_t<typename T::Row>> {
	using type = typename T::Row;
};
template<typename T>
using row_t = typename get_row<T>::type;
using StatementHandle = unique_resource<sqlite3_stmt*, detail::StatementDeleter>;
}

using DB = unique_resource<sqlite3*, detail::DBDeleter>;
template<typename Def>
struct Statement {
	using Params = detail::params_t<Def>;
	using Row = detail::row_t<Def>;
	detail::StatementHandle handle;
};

namespace detail {
enum class QueryStatus: int {
	Done = 101, // SQLITE_DONE
	Row = 100, // SQLITE_ROW
};

auto prepare_raw(DB&, string_view query) -> StatementHandle;

template<typename T>
           void bind                  (StatementHandle&, int idx, T arg);
template<> void bind<int>             (StatementHandle&, int idx, int arg);
template<> void bind<int64_t>         (StatementHandle&, int idx, int64_t arg);
template<> void bind<double>          (StatementHandle&, int idx, double arg);
template<> void bind<string_view>     (StatementHandle&, int idx, string_view arg);
template<> void bind<span<byte const>>(StatementHandle&, int idx, span<byte const> arg);
template<> void bind<void const*>     (StatementHandle&, int idx, void const* arg);

auto step(StatementHandle&) -> QueryStatus;
void reset(StatementHandle&);
auto last_insert_rowid(StatementHandle&) -> int64_t;
auto acquire_db_mutex(DB&) -> detail::Mutex;

template<typename T>
           auto get_column                  (StatementHandle&, int idx) -> T;
template<> auto get_column<int>             (StatementHandle&, int idx) -> int;
template<> auto get_column<int64_t>         (StatementHandle&, int idx) -> int64_t;
template<> auto get_column<double>          (StatementHandle&, int idx) -> double;
template<> auto get_column<string_view>     (StatementHandle&, int idx) -> string_view;
template<> auto get_column<span<byte const>>(StatementHandle&, int idx) -> span<byte const>;
template<> auto get_column<void const*>     (StatementHandle&, int idx) -> void const*;

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

// Compile a query definition into a statement object.
// Throws runtime_error on sqlite error.
template<typename Def>
auto prepare(DB&) -> Statement<Def>;

// Execute a single SQL query on the database, discarding any output.
// Throws runtime_error on sqlite error.
void execute(DB&, string_view query);

// Execute a list of SQL queries on the database, discarding any output. This must be used for
// executions that contain multiple statements, as the single-statement version doesn't support
// splitting by the ";" character. The queries are automatically bundled into a transaction.
// Throws runtime_error on sqlite error.
void execute(DB&, span<string_view const> query);

// Execute a statement with provided parameters, discarding any output data.
// Throws runtime_error on sqlite error.
template<typename Def, typename... Args>
void execute(Statement<Def>&, Args&&... args);

// Execute an insert statement with provided parameters, and return the rowid of the inserted row.
// Throws runtime_error on sqlite error.
template<typename Def, typename... Args>
auto insert(Statement<Def>&, Args&&... args) -> int64_t;

// Execute a statement with provided parameters, and call the provided function on every resulting row.
// Throws runtime_error on sqlite error.
template<typename Def, typename... Args>
auto query(Statement<Def>&, Args&&... args) -> generator<typename Statement<Def>::Row>;

// Bundle queries into an atomic transaction. All queries executed within the provided function
// will be executed wholly or not at all.
// Throws runtime_error on sqlite error.
template<callable<void()> Func>
void transaction(DB&, Func&&);

template<typename Def>
auto prepare(DB& db) -> Statement<Def>
{
	auto handle = detail::prepare_raw(db, Def::Query);
	return Statement<Def>{move(handle)};
}

template<typename Def, typename... Args>
void execute(Statement<Def>& stmt, Args&&... args)
{
	for (auto _: query(stmt, forward<Args>(args)...)) {}
}

template<typename Def, typename... Args>
auto insert(Statement<Def>& stmt, Args&&... args) -> int64_t
{
	execute(stmt, forward<Args>(args)...);
	return detail::last_insert_rowid(stmt.handle);
}

template<typename Def, typename... Args>
auto query(Statement<Def>& stmt, Args&&... args) -> generator<typename Statement<Def>::Row>
{
	using Params = typename Statement<Def>::Params;
	using Row = typename Statement<Def>::Row;
	static_assert(sizeof...(Args) == tuple_size_v<Params>);

	struct Resetter {
		detail::StatementHandle& h;
		~Resetter() { detail::reset(h); }
	};
	auto resetter = Resetter{stmt.handle};

	// Bind statement parameters
	[[maybe_unused]] auto bind = [&](int idx, auto&& arg) {
		using ArgT = remove_cvref_t<decltype(arg)>;
		if constexpr (same_as<ArgT, float> || same_as<ArgT, double>)
			detail::bind<double>(stmt.handle, idx, arg);
		else if constexpr (same_as<ArgT, int64_t> || same_as<ArgT, uint64_t>)
			detail::bind<int64_t>(stmt.handle, idx, arg);
		else if constexpr (convertible_to<ArgT, int> || same_as<ArgT, bool>)
			detail::bind<int>(stmt.handle, idx, arg);
		else if constexpr (same_as<ArgT, char const*> || same_as<ArgT, string> || same_as<ArgT, string_view>)
			detail::bind<string_view>(stmt.handle, idx, arg);
		else if constexpr (convertible_to<ArgT, span<byte const>> || convertible_to<ArgT, span<unsigned char const>>)
			detail::bind<span<byte const>>(stmt.handle, idx, {reinterpret_cast<byte const*>(arg.data()), arg.size()});
		else if constexpr (same_as<ArgT, void*> || same_as<ArgT, void const*>)
			detail::bind<void const*>(stmt.handle, idx, arg);
		else
			static_assert(false, "Unknown sqlite binding type");
	};
	auto index = 1;
	(bind(index++, forward<Args>(args)), ...);

	// Step through every result row
	while (detail::step(stmt.handle) == detail::QueryStatus::Row) {
		auto row = [&]<size_t... I>(index_sequence<I...>) {
			return make_tuple(detail::get_column<tuple_element_t<I, Row>>(stmt.handle, I)...);
		}(make_index_sequence<tuple_size_v<Row>>{});
		co_yield move(row);
	}
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

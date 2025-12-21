/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include <cstdlib>

#include "preamble.hpp"
#include "io/file.hpp"
#include "lib/sqlite.hpp"

using namespace playnote;

struct InsertAsset {
	static constexpr auto Query = R"sql(
		INSERT INTO assets(id, data) VALUES(?1, ?2)
	)sql";
	using Params = tuple<uint, span<byte const>>;
};

auto main(int argc, char** argv) -> int
try {
	auto args = span{argv, static_cast<size_t>(argc)};
	if (args.size() < 3) {
		print(stderr, "Usage: {} <output database> <input assets>...\n", args[0]);
		return EXIT_FAILURE;
	}
	auto* out_filename = args[1];
	auto in_filenames = args.subspan(2);

	if (fs::exists(out_filename)) fs::remove(out_filename);
	auto db = lib::sqlite::open(out_filename);
	lib::sqlite::execute(db, R"sql(
		CREATE TABLE IF NOT EXISTS assets(
			id INTEGER PRIMARY KEY,
			data BLOB NOT NULL
		)
	)sql");

	auto stmt = lib::sqlite::prepare<InsertAsset>(db);
	for (auto path_sv: in_filenames) {
		auto path = fs::path{path_sv};
		auto filename = path.filename().string();
		auto filename_id = id{filename};
		auto file = io::read_file(path);
		lib::sqlite::execute(stmt, +filename_id, file.contents);
	}

} catch (exception const& e) {
	print(stderr, "Uncaught exception: {}", e.what());
	return EXIT_FAILURE;
}

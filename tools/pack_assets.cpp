/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "preamble.hpp"
#include "io/file.hpp"
#include "lib/sqlite.hpp"
#include "lib/zstd.hpp"

namespace playnote {

static constexpr auto AssetsSchema = R"sql(
	CREATE TABLE assets(
		id INTEGER PRIMARY KEY,
		compressed INTEGER NOT NULL,
		data BLOB NOT NULL
	)
)sql";

struct InsertAsset {
	static constexpr auto Query = R"sql(
		INSERT INTO assets(id, compressed, data) VALUES(?1, ?2, ?3)
	)sql";
	using Params = tuple<uint, int, span<byte const>>;
};

auto pack_assets(span<char const* const> args)
try {
	if (args.size() < 3) {
		print(stderr, "Usage: {} <output database> <input assets>...\nInput asset: <path>[:z]", args[0]);
		return EXIT_FAILURE;
	}
	auto* out_filename = args[1];
	auto in_filenames = args.subspan(2);

	if (fs::exists(out_filename)) fs::remove(out_filename);
	auto db = lib::sqlite::open(out_filename);
	lib::sqlite::execute(db, AssetsSchema);

	auto stmt = lib::sqlite::prepare<InsertAsset>(db);
	for (auto path_cstr: in_filenames) {
		auto path_sv = string_view{path_cstr};
		auto compress = false;
		if (path_sv.ends_with(":z")) {
			compress = true;
			path_sv.remove_suffix(2);
		}
		auto path = fs::path{path_sv};
		auto filename = path.filename().string();
		auto filename_id = id{filename};
		auto file = io::read_file(path);
		lib::sqlite::execute(stmt, +filename_id, compress,
			compress? lib::zstd::compress(file.contents, lib::zstd::CompressionLevel::Ultra) : file.contents);
	}
	return EXIT_SUCCESS;
}
catch (exception const& e) {
	print(stderr, "Uncaught exception: {}", e.what());
	return EXIT_FAILURE;
}

}

auto main(int argc, char** argv) -> int
{ return playnote::pack_assets({argv, static_cast<std::size_t>(argc)}); }

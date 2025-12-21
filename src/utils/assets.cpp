/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "utils/assets.hpp"

#include "preamble.hpp"
#include "lib/sqlite.hpp"
#include "utils/logger.hpp"

namespace playnote {

Assets::Assets(fs::path const& db_path)
{
	if (!fs::exists(db_path)) throw runtime_error_fmt("Asset database is missing at \"{}\"", db_path);
	db = lib::sqlite::open(db_path);
	select_asset = lib::sqlite::prepare<SelectAsset>(db);
	INFO("Opened asset database at \"{}\"", db_path);
}

auto Assets::get(id asset_id) -> vector<byte>
{
	for (auto [data]: lib::sqlite::query(select_asset, +asset_id))
		return {data.begin(), data.end()};
	throw runtime_error_fmt("Asset ID {} not found", +asset_id);
}

}

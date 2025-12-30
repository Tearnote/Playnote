/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "lib/sqlite.hpp"
#include "utils/service.hpp"

namespace playnote {

class Assets {
public:
	explicit Assets(fs::path const& db_path);

	auto get(id) -> vector<byte>;

private:
	struct SelectAsset {
		static constexpr auto Query = R"sql(
			SELECT compressed, data FROM assets WHERE id = ?1
		)sql";
		using Params = tuple<uint>;
		using Row = tuple<int, span<byte const>>;
	};

	lib::sqlite::DB db;
	lib::sqlite::Statement<SelectAsset> select_asset;
};

namespace globals {
inline auto assets = Service<Assets>{};
}

}

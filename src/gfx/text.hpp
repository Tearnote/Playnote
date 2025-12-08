/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "utils/logger.hpp"
#include "lib/harfbuzz.hpp"
#include "io/file.hpp"

namespace playnote::gfx {

class TextShaper {
public:
	TextShaper(Logger::Category);

	void load_font(io::ReadFile&&, initializer_list<int> weights = {500});

private:
	Logger::Category cat;
	lib::harfbuzz::Context ctx;
	vector<lib::harfbuzz::Font> fonts;
};

inline TextShaper::TextShaper(Logger::Category cat):
	cat{cat},
	ctx{lib::harfbuzz::init()}
{}

inline void TextShaper::load_font(io::ReadFile&& file, initializer_list<int> weights)
{
	auto file_ptr = make_shared<io::ReadFile>(move(file));
	for (auto weight: weights)
		fonts.emplace_back(lib::harfbuzz::create_font(ctx, file_ptr, weight));
	INFO_AS(cat, "Loaded font at \"{}\"", file.path);
}

}

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
	TextShaper(Logger::Category, initializer_list<string_view> fonts);

private:
	Logger::Category cat;
	lib::harfbuzz::Context ctx;
	vector<lib::harfbuzz::Font> fonts;
};

inline TextShaper::TextShaper(Logger::Category cat, initializer_list<string_view> fonts):
	cat{cat},
	ctx{lib::harfbuzz::init()}
{
	for (auto font: fonts) {
		this->fonts.emplace_back(lib::harfbuzz::create_font(ctx, io::read_file(font)));
		INFO_AS(cat, "Loaded font at \"{}\"", font);
	}
}

}

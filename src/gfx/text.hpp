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
#include "lib/icu.hpp"
#include "io/file.hpp"

namespace playnote::gfx {

class TextShaper {
public:
	TextShaper(Logger::Category);

	void load_font(id, io::ReadFile&&, initializer_list<int> weights = {500});
	void define_style(id, initializer_list<id> fonts, int weight = 500);
	void shape(id style_id, string_view text); //TODO return value

private:
	Logger::Category cat;
	lib::harfbuzz::Context ctx;
	unordered_node_map<pair<id, int>, lib::harfbuzz::Font> fonts; // key: font id, weight
	unordered_map<id, vector<reference_wrapper<lib::harfbuzz::Font>>> styles;
};

inline TextShaper::TextShaper(Logger::Category cat):
	cat{cat},
	ctx{lib::harfbuzz::init()}
{}

inline void TextShaper::load_font(id font_id, io::ReadFile&& file, initializer_list<int> weights)
{
	auto file_ptr = make_shared<io::ReadFile>(move(file));
	for (auto weight: weights)
		fonts.emplace(make_pair(font_id, weight), lib::harfbuzz::create_font(ctx, file_ptr, weight));
	INFO_AS(cat, "Loaded font at \"{}\"", file_ptr->path);
}

inline void TextShaper::define_style(id style_id, initializer_list<id> fonts, int weight)
{
	auto style_fonts = vector<reference_wrapper<lib::harfbuzz::Font>>{};
	style_fonts.reserve(fonts.size());
	transform(fonts, back_inserter(style_fonts), [&](auto font_id) {
		return ref(this->fonts.at(make_pair(font_id, weight)));
	});
	styles.emplace(style_id, move(style_fonts));
}

inline void TextShaper::shape(id style_id, string_view text)
{
	auto const& style_fonts = styles.at(style_id);
	for (auto cluster: lib::icu::grapheme_clusters(text))
		TRACE_AS(cat, "Cluster: {}", cluster);
}

}

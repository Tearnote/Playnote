/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "preamble/algorithm.hpp"
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
	using FontRef = reference_wrapper<lib::harfbuzz::Font>;
	using FontRefConst = reference_wrapper<lib::harfbuzz::Font const>;

	Logger::Category cat;
	lib::harfbuzz::Context ctx;
	unordered_node_map<pair<id, int>, lib::harfbuzz::Font> fonts; // key: font id, weight
	unordered_map<id, vector<FontRef>> styles;

	using Run = pair<string_view, FontRefConst>;
	auto itemize(string_view, span<FontRef const>) -> generator<Run>;
	auto itemize(string&&, span<FontRef const>) -> generator<Run> = delete;
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
	auto style_fonts = vector<FontRef>{};
	style_fonts.reserve(fonts.size());
	transform(fonts, back_inserter(style_fonts), [&](auto font_id) {
		return ref(this->fonts.at(make_pair(font_id, weight)));
	});
	styles.emplace(style_id, move(style_fonts));
}

inline void TextShaper::shape(id style_id, string_view text)
{
	auto const& style_fonts = styles.at(style_id);
	for (auto [run, font]: itemize(text, style_fonts))
			TRACE_AS(cat, "Run: \"{}\"", run);
}

inline auto TextShaper::itemize(string_view text, span<FontRef const> fonts) -> generator<Run>
{
	auto current_font = optional<FontRef>{nullopt};
	auto run_start = text.begin();

	for (auto cluster: lib::icu::grapheme_clusters(text)) {
		auto best_font = optional<FontRef>{nullopt};
		for (auto font: fonts) {
			auto font_suitable = true;
			for (auto scalar: lib::icu::scalars(cluster)) {
				if (!lib::harfbuzz::has_glyph(font.get(), scalar)) {
					font_suitable = false;
					break;
				}
			}
			if (font_suitable) {
				best_font = font;
				break;
			}
		}

		if (!best_font) {
			WARN_AS(cat, "No font supports the character \"{}\"", cluster);
			// Extend the current run; the character won't be rendered, but there's no point
			// in splitting the run because of it.
			continue;
		}
		if (!current_font) {
			// Starting the first run
			current_font = best_font;
			continue;
		}
		if (current_font->get().get() == best_font->get().get()) continue;

		// The ongoing run is over
		co_yield {string_view{run_start, cluster.begin()}, *current_font};
		current_font = best_font;
		run_start = cluster.begin();
	}

	// Final run
	if (current_font) co_yield {string_view{run_start, text.end()}, *current_font};
}

}

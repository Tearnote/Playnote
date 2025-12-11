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
	using FontID = id;
	using StyleID = id;

	TextShaper(Logger::Category);

	void load_font(FontID, io::ReadFile&&, initializer_list<int> weights = {500});
	void define_style(StyleID, initializer_list<FontID>, int weight = 500);
	void shape(StyleID, string_view); //TODO return value

private:
	using FontRef = reference_wrapper<lib::harfbuzz::Font>;
	using CacheKey = tuple<FontID, int, ssize_t>; // font id, weight, glyph index

	struct CachedGlyph {
		AABB<float2> atlas_bounds;
		float2 bearing; // position of glyph origin within the atlas bounds
	};

	Logger::Category cat;
	lib::harfbuzz::Context ctx;
	unordered_map<pair<FontID, int>, lib::harfbuzz::Font> fonts; // key: font id, weight
	unordered_map<StyleID, pair<vector<FontID>, int>> styles; // value: font cascade by id, weight
	unordered_map<CacheKey, CachedGlyph> atlas_cache;

	using Run = pair<string_view, ssize_t>;
	auto itemize(string_view, span<FontRef const>) -> generator<Run>;
	auto itemize(string&&, span<FontRef const>) -> generator<Run> = delete;
};

inline TextShaper::TextShaper(Logger::Category cat):
	cat{cat},
	ctx{lib::harfbuzz::init()}
{}

inline void TextShaper::load_font(FontID font_id, io::ReadFile&& file, initializer_list<int> weights)
{
	auto file_ptr = make_shared<io::ReadFile>(move(file));
	for (auto weight: weights)
		fonts.emplace(make_pair(font_id, weight), lib::harfbuzz::create_font(ctx, file_ptr, weight));
	INFO_AS(cat, "Loaded font at \"{}\"", file_ptr->path);
}

inline void TextShaper::define_style(StyleID style_id, initializer_list<FontID> fonts, int weight)
{
	auto style_fonts = vector<FontID>{};
	style_fonts.reserve(fonts.size());
	copy(fonts, back_inserter(style_fonts));
	styles.emplace(style_id, pair{move(style_fonts), weight});
}

inline void TextShaper::shape(id style_id, string_view text)
{
	auto const [style_font_ids, weight] = styles.at(style_id);
	auto style_font_refs = vector<FontRef>{};
	style_font_refs.reserve(style_font_ids.size());
	transform(style_font_ids, back_inserter(style_font_refs), [&](auto fid) { return ref(fonts.at({fid, weight})); });

	// Collect shaping results
	struct Run {
		lib::harfbuzz::ShapedRun shaped_run;
		FontID font_id;
		float2 position;
	};
	auto runs = vector<Run>{};
	auto cursor = float2{0.0f, 0.0f};
	for (auto [run_str, font_idx]: itemize(text, style_font_refs)) {
		auto const& font = style_font_refs[font_idx];
		auto shaped_run = lib::harfbuzz::shape(text, run_str, font);
		auto& run = runs.emplace_back(move(shaped_run), style_font_ids[font_idx], cursor);
		cursor += run.shaped_run.advance;
	}

	// Flatten shaping results to a list of glyphs
	struct PendingGlyph {
		CacheKey key;
		float2 position;
	};
	auto pending_glyphs = vector<PendingGlyph>{};
	pending_glyphs.reserve(fold_left(runs, 0z, [](auto acc, auto const& run) { return acc + run.shaped_run.glyphs.size(); }));
	for (auto const& run: runs) {
		for (auto const& glyph: run.shaped_run.glyphs)
			pending_glyphs.emplace_back(CacheKey{run.font_id, weight, glyph.idx}, run.position + glyph.offset);
	}

	// Update atlas with missing glyphs
	auto missing_keys = vector<CacheKey>{};
	missing_keys.reserve(pending_glyphs.size());
	transform(
		pending_glyphs | views::filter([&](auto const& g) { return !atlas_cache.contains(g.key); }),
		back_inserter(missing_keys),
		[](auto const& g) { return g.key; }
	);
	sort(missing_keys);
	auto removed = unique(missing_keys);
	TRACE_AS(cat, "duplicate keys: {}", removed.size());
	missing_keys.erase(removed.begin(), removed.end());
	//TODO rasterize missing glyphs into atlas

	for (auto const& missing: missing_keys)
		TRACE_AS(cat, "missing: font {}, weight {}, idx {}",
			+get<FontID>(missing), get<int>(missing), get<ssize_t>(missing));

	//TODO get atlas UVs for each glyph
	//TODO compute AABB bound relative to origin
	//TODO return result buffer
}

inline auto TextShaper::itemize(string_view text, span<FontRef const> fonts) -> generator<Run>
{
	ASSUME(!fonts.empty());

	auto current_font_idx = optional<ssize_t>{nullopt};
	auto run_start = text.begin();
	auto scalars = small_vector<char32_t, 8>{};

	for (auto cluster: lib::icu::grapheme_clusters(text)) {
		auto best_font_idx = optional<ssize_t>{nullopt};
		scalars.clear();
		copy(lib::icu::scalars(cluster), back_inserter(scalars));

		// Use current font for whitespace if possible
		if (current_font_idx && all_of(scalars, [](auto s) { return lib::icu::is_whitespace(s); })) {
			auto current_suitable = true;
			for (auto scalar: scalars) {
				if (!lib::harfbuzz::has_glyph(fonts[*current_font_idx], scalar)) {
					current_suitable = false;
					break;
				}
			}
			if (current_suitable) best_font_idx = current_font_idx;
		}

		// Locate first supported font
		if (!best_font_idx) {
			for (auto [font_idx, font]: fonts | views::enumerate) {
				auto font_suitable = true;
				for (auto scalar: scalars) {
					if (!lib::harfbuzz::has_glyph(font.get(), scalar)) {
						font_suitable = false;
						break;
					}
				}
				if (font_suitable) {
					best_font_idx = font_idx;
					break;
				}
			}
		}

		if (!best_font_idx) {
			WARN_AS(cat, "No font supports the character \"{}\"", cluster);
			// Extend the current run; the character won't be rendered, but there's no point
			// in splitting the run because of it.
			continue;
		}
		if (!current_font_idx) {
			// Starting the first run
			current_font_idx = best_font_idx;
			continue;
		}
		if (current_font_idx == best_font_idx) continue;

		// The ongoing run is over
		co_yield {string_view{run_start, cluster.begin()}, *current_font_idx};
		current_font_idx = best_font_idx;
		run_start = cluster.begin();
	}

	// Final run
	if (current_font_idx)
		co_yield {string_view{run_start, text.end()}, *current_font_idx};
	else
		co_yield {text, 0}; // No character was supported, use primary font
}

}

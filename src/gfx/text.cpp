/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "gfx/text.hpp"

#include "preamble.hpp"
#include "lib/bits.hpp"
#include "lib/icu.hpp"

namespace playnote::gfx {

TextShaper::TextShaper(Logger::Category cat):
	cat{cat},
	ctx{lib::harfbuzz::init()},
	dynamic_atlas{256}
{ dynamic_atlas.atlasGenerator().setThreadCount(max(1u, jthread::hardware_concurrency() - 2u)); }

void TextShaper::load_font(FontID font_id, vector<byte>&& data, int weight)
{
	auto const& font_data = this->font_data.emplace_back(move(data));
	fonts.emplace(make_pair(font_id, weight), lib::harfbuzz::create_font(ctx, font_data));
}

void TextShaper::define_style(StyleID style_id, initializer_list<FontID> fonts, int weight)
{ define_style(style_id, {fonts.begin(), fonts.end()}, weight); }

void TextShaper::define_style(StyleID style_id, span<FontID const> fonts, int weight)
{
	auto style_fonts = vector<FontID>{};
	style_fonts.reserve(fonts.size());
	copy(fonts, back_inserter(style_fonts));
	styles.emplace(style_id, pair{move(style_fonts), weight});
}

auto TextShaper::shape(id style_id, string_view text) -> Text
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

		// Scale to atlas pixels
		auto const scale = PixelsPerEm / lib::harfbuzz::units_per_em(font);
		for (auto& glyph: run.shaped_run.glyphs)
			glyph.offset *= scale;
		run.shaped_run.advance *= scale;

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
	missing_keys.erase(removed.begin(), removed.end());
	cache_glyphs(missing_keys);

	// Collect results
	auto result = Text{};
	result.bounds.top_left.x() = numeric_limits<float>::max();
	result.bounds.top_left.y() = numeric_limits<float>::max();
	result.bounds.bottom_right.x() = numeric_limits<float>::lowest();
	result.bounds.bottom_right.y() = numeric_limits<float>::lowest();
	result.glyphs.reserve(pending_glyphs.size());
	for (auto const& pending: pending_glyphs) {
		auto [page, layout] = atlas_cache.at(pending.key);
		if (layout.atlas_bounds.size() == float2{0.0f, 0.0f}) continue; // whitespace
		auto const offset = pending.position - layout.bearing;
		result.glyphs.emplace_back(layout.atlas_bounds, offset, page);
		result.bounds.top_left = min(result.bounds.top_left, offset);
		result.bounds.bottom_right = max(result.bounds.bottom_right, offset + layout.atlas_bounds.size());
	}
	return result;
}

auto TextShaper::get_atlas(ssize_t page) -> lib::msdf::AtlasView
{
	if (page == 0) return static_atlas;

	atlas_dirty = false;
	return lib::msdf::get_atlas_contents(dynamic_atlas);
}

void TextShaper::dump_atlas(fs::path const& path) const
try {
	lib::msdf::atlas_to_image(dynamic_atlas, path);
	INFO_AS(cat, "Exported font atlas to \"{}\"", path);
}
catch (exception const&) {
	WARN_AS(cat, "Failed to export font atlas");
}

auto TextShaper::serialize() -> vector<byte>
{
	// Binary archive contents are:
	// 1. bitmap dimensions
	// 2. raw bitmap bytes
	// 3. layout cache

	auto data = vector<byte>{};
	auto out = lib::bits::out{data};
	auto view = lib::msdf::get_atlas_contents(dynamic_atlas);

	auto const* shape = view.shape();
	out(shape[0], shape[1], shape[2]).or_throw();
	out(span{view.data(), view.num_elements()}).or_throw();
	out(atlas_cache).or_throw();

	return data;
}

void TextShaper::deserialize(span<byte const> data)
{
	auto in = lib::bits::in{data};

	using size_type = decltype(static_atlas)::size_type;
	auto width = size_type{};
	auto height = size_type{};
	auto channels = size_type{};
	in(width, height, channels).or_throw();

	static_atlas.resize(extents[width][height][channels]);
	in(span{static_atlas.data(), static_atlas.num_elements()}).or_throw();
	in(atlas_cache).or_throw();

	// Rewrite page index
	for (auto& [_, value]: atlas_cache) value.first = 0;

	atlas_dirty = true;
}

auto TextShaper::itemize(string_view text, span<FontRef const> fonts) -> generator<Run>
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

void TextShaper::cache_glyphs(span<CacheKey const> glyph_keys)
{
	auto glyphs = vector<lib::msdf::GlyphGeometry>{};
	auto keys = vector<CacheKey>{};
	glyphs.reserve(glyph_keys.size());
	keys.reserve(glyph_keys.size());

	// Load glyph geometries from each font
	for (auto chunk: glyph_keys | views::chunk_by([](auto const& a, auto const& b) {
		return get<FontID>(a) == get<FontID>(b) && get<int>(a) == get<int>(b);
	})) {
		auto [font_id, weight, _] = chunk.front();
		auto const& font_resource = fonts.at({font_id, weight});
		auto loader = lib::msdf::GlyphLoader{font_resource, PixelsPerEm};
		for (auto const& key: chunk) {
			auto const glyph_idx = get<ssize_t>(key);
			if (auto glyph = loader.load_glyph(glyph_idx)) {
				glyphs.emplace_back(move(*glyph));
				keys.emplace_back(key);
			} else {
				// Failed to load; cache as empty to prevent re-loading attempts
				atlas_cache.emplace(key, pair{1, lib::msdf::GlyphLayout{}});
			}
		}
	}

	// Rasterize and pack
	auto layouts = lib::msdf::add_glyphs(dynamic_atlas, glyphs);

	// Update cache
	for (auto [layout, key]: views::zip(layouts, keys))
		atlas_cache.emplace(key, pair{1, layout});

	atlas_dirty = true;
}

}

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
#include "lib/msdf.hpp"

namespace playnote::gfx {

// Performs text shaping and manages a glyph atlas.
class TextShaper {
public:
	static constexpr auto PixelsPerEm = 64.0f;
	static constexpr auto DistanceRange = 8.0f;

	using FontID = id;
	using StyleID = id;

	// Visual representation of a line of text, renderable with glyphs in the atlas.
	struct Text {
		struct Glyph {
			AABB<float> atlas_bounds;
			float2 offset; // from origin at (0, 0), the start of the baseline
			int page;
		};

		struct Line {
			vector<Glyph> glyphs;
			AABB<float> bounds; // can extend to the left of origin, or even not contain origin at all
		};

		vector<Line> lines;
	};

	// Initialize with an empty atlas. The optional initial size is the size of both of the atlas'
	// sides, in pixels.
	explicit TextShaper(Logger::Category, int initial_size = 256);

	// Add a font file into available fonts at the specified weight.
	void load_font(FontID, vector<byte>&&, int weight);

	// Add a style, which is a font fallback cascade at a specified weight. The fonts must all
	// have been previously added with that exact weight.
	void define_style(StyleID, initializer_list<FontID>, int weight = 500);
	void define_style(StyleID, span<FontID const>, int weight = 500);

	// Shape text into glyphs using the specified style. The returned object can be used repeatedly.
	// If max_width is set, lines will be wrapped to fit within that width.
	auto shape(StyleID, string_view, optional<float> max_width = nullopt) -> Text;

	// Return true if the dynamic atlas bitmap has changed since the last call to get_atlas().
	auto is_atlas_dirty() const noexcept -> bool { return atlas_dirty; }

	// Return the current atlas bitmap. The optional page number selects the atlas; the static atlas
	// is page 0, and dynamic one page 1. The static atlas can only change by a call to deserialize().
	auto get_atlas(ssize_t page = 1) -> lib::msdf::AtlasView;

	// Save the dynamic atlas to a file, for debugging purposes.
	void dump_atlas(fs::path const&) const;

	// Turn dynamic atlas state into a binary representation, which can later be restored with
	// deserialize() into the static atlas to accelerate atlas generation of the most common glyphs.
	auto serialize() -> vector<byte>;

	// Restore the atlas state from a previously serialized binary.
	// The same fonts and styles must have been created as what existed as serialization time.
	// This call is invalid if any text has been shaped and the dynamic atlas is not empty.
	void deserialize(span<byte const>);

private:
	using FontRef = reference_wrapper<lib::harfbuzz::Font>;
	using CacheKey = tuple<FontID, int, ssize_t>; // font id, weight, glyph index

	struct PendingGlyph {
		CacheKey key;
		uint cluster;
		float2 position;
	};

	Logger::Category cat;
	lib::harfbuzz::Context ctx;
	vector<vector<byte>> font_data;
	unordered_map<pair<FontID, int>, lib::harfbuzz::Font> fonts; // key: font id, weight
	unordered_map<StyleID, pair<vector<FontID>, int>> styles; // value: font cascade by id, weight
	multi_array<byte, 3> static_atlas;
	lib::msdf::MTSDFAtlas dynamic_atlas;
	unordered_map<CacheKey, pair<ssize_t, lib::msdf::GlyphLayout>> atlas_cache; // value: atlas page (0 = static), glyph layout
	bool atlas_dirty = true;

	using Run = pair<string_view, ssize_t>;
	auto generate_lines(string_view, StyleID, optional<float> max_width) -> generator<vector<PendingGlyph>>;
	auto shape_paragraph(string_view, int weight, span<FontID const>, span<FontRef const>,
		optional<float> max_width) -> generator<vector<PendingGlyph>>;
	auto itemize(string_view, span<FontRef const>) -> generator<Run>;
	auto itemize(string&&, span<FontRef const>) -> generator<Run> = delete;
	void cache_glyphs(span<CacheKey const>);
};

using Text = TextShaper::Text;
using Glyph = TextShaper::Text::Glyph;

}

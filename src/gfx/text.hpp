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
	using FontID = id;
	using StyleID = id;

	// Visual representation of a line of text, renderable with glyphs in the atlas.
	struct Text {
		struct Glyph {
			AABB<float> atlas_bounds;
			float2 offset; // from origin at (0, 0), the start of the baseline
		};

		vector<Glyph> glyphs;
		AABB<float> bounds; // can extend to the left of origin, or even not contain origin at all
	};

	// Initialize with an empty atlas.
	explicit TextShaper(Logger::Category);

	// Add a font file into available fonts at the specified weight.
	void load_font(FontID, vector<byte>&&, int weight);

	// Add a style, which is a font fallback cascade at a specified weight. The fonts must all
	// have been previously added with that exact weight.
	void define_style(StyleID, initializer_list<FontID>, int weight = 500);
	void define_style(StyleID, span<FontID const>, int weight = 500);

	// Shape text into glyphs using the specified style. The returned object can be used repeatedly.
	auto shape(StyleID, string_view) -> Text;

	// Return true if the atlas bitmap has changed since the last call to get_atlas().
	auto is_atlas_dirty() const noexcept -> bool { return atlas_dirty; }

	// Return the current atlas bitmap.
	auto get_atlas() -> lib::msdf::AtlasView;

	// Save the atlas to a file for debugging purposes.
	void dump_atlas(fs::path const&) const;

	// Turn atlas state into a binary representation, which can later be restored with deserialize().
	// The two return types are the bitmap and the layout, respectively.
	// The state does not include loaded fonts or created styles.
	auto serialize() -> pair<vector<byte>, vector<byte>>;

	// Restore the atlas state from a previously serialized binary.
	// The same fonts and styles must have been created as what existed as serialization time.
	void deserialize(span<byte const> bitmap, span<byte const> layout);

private:
	using FontRef = reference_wrapper<lib::harfbuzz::Font>;
	using CacheKey = tuple<FontID, int, ssize_t>; // font id, weight, glyph index

	static constexpr auto PixelsPerEm = 32.0f;

	Logger::Category cat;
	lib::harfbuzz::Context ctx;
	vector<vector<byte>> font_data;
	unordered_map<pair<FontID, int>, lib::harfbuzz::Font> fonts; // key: font id, weight
	unordered_map<StyleID, pair<vector<FontID>, int>> styles; // value: font cascade by id, weight
	lib::msdf::MTSDFAtlas atlas;
	unordered_map<CacheKey, lib::msdf::GlyphLayout> atlas_cache;
	bool atlas_dirty = true;

	using Run = pair<string_view, ssize_t>;
	auto itemize(string_view, span<FontRef const>) -> generator<Run>;
	auto itemize(string&&, span<FontRef const>) -> generator<Run> = delete;
	void cache_glyphs(span<CacheKey const>);
};

using Text = TextShaper::Text;
using Glyph = TextShaper::Text::Glyph;

}

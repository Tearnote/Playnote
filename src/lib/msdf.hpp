/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include <ft2build.h>
#include FT_FREETYPE_H // Required for MSDF headers to include Freetype related functions
#include "msdf-atlas-gen/msdf-atlas-gen.h"
#include "preamble.hpp"
#include "lib/harfbuzz.hpp"

// Forward declarations

namespace msdfgen {
class FontHandle;
};

namespace playnote::lib::msdf {

// The curves defining a glyph; opaque type
using msdf_atlas::GlyphGeometry;

// A font atlas holding MTSDF (MSDF in RGB, SDF in alpha) data. Constructor optionally takes in
// the initial size of the atlas in pixels.
using MTSDFAtlas = msdf_atlas::DynamicAtlas<
	msdf_atlas::ImmediateAtlasGenerator<
		float, 4, &msdf_atlas::mtsdfGenerator,
		msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>
	>
>;

// View into the atlas' pixel array.
using AtlasView = const_multi_array_ref<byte, 3>;

// Extractor of glyph data from a font.
class GlyphLoader {
public:
	// Create the loader. Font resource must exist for its entire lifetime.
	// pixels_per_em controls the scale of extracted glyph geometry.
	GlyphLoader(lib::harfbuzz::Font const&, float pixels_per_em);
	~GlyphLoader() noexcept;

	// Load a glyph with the specified index. Returns nullopt on error. Whitespace will return
	// an empty, but valid glyph.
	auto load_glyph(ssize_t) -> optional<GlyphGeometry>;

	GlyphLoader(GlyphLoader const&) = delete;
	auto operator=(GlyphLoader const&) -> GlyphLoader& = delete;
	GlyphLoader(GlyphLoader&&) = delete;
	auto operator=(GlyphLoader&&) -> GlyphLoader& = delete;

private:
	lib::harfbuzz::Font const& ft_font;
	msdfgen::FontHandle* font;
	float pixels_per_em;
};

// How a glyph is positioned within the atlas.
struct GlyphLayout {
	AABB<float> atlas_bounds;
	float2 bearing; // position of glyph origin, relative to atlas_bounds.top_left
};

// Expand the atlas with additional glyphs. The new glyphs' atlas positions are returned,
// with indices matching input data.
auto add_glyphs(MTSDFAtlas&, span<GlyphGeometry>) -> vector<GlyphLayout>;

// Return atlas contents as a read-only view.
auto get_atlas_contents(MTSDFAtlas const&) -> AtlasView;

// Set atlas contents from a read-only view.
void set_atlas_contents(MTSDFAtlas&, AtlasView);

// Save atlas contents to a file.
void atlas_to_image(MTSDFAtlas const&, fs::path const&);

}

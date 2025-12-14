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

namespace msdfgen {
class FontHandle;
};

namespace playnote::lib::msdf {

using msdf_atlas::GlyphGeometry;
using MTSDFAtlas = msdf_atlas::DynamicAtlas<
	msdf_atlas::ImmediateAtlasGenerator<
		float, 4, &msdf_atlas::mtsdfGenerator,
		msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>
	>
>;
using AtlasView = const_multi_array_ref<byte, 3>;

class GlyphLoader {
public:
	GlyphLoader(lib::harfbuzz::Font const&, float pixels_per_em);
	~GlyphLoader() noexcept;

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

struct GlyphLayout {
	AABB<float2> atlas_bounds;
	float2 bearing;
};

auto add_glyphs(MTSDFAtlas&, span<GlyphGeometry>) -> vector<GlyphLayout>;

auto get_atlas_contents(MTSDFAtlas const&) -> AtlasView;

void atlas_to_image(MTSDFAtlas const&, fs::path const&);

}

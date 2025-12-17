/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "lib/msdf.hpp"

#include "msdfgen/ext/import-font.h"
#include "msdf-atlas-gen/image-save.h"
#include "preamble.hpp"

namespace playnote::lib::msdf {

GlyphLoader::GlyphLoader(lib::harfbuzz::Font const& ft_font, float pixels_per_em):
	ft_font{ft_font},
	font{msdfgen::adoptFreetypeFont(ft_font->face)},
	pixels_per_em{pixels_per_em}
{}

GlyphLoader::~GlyphLoader() noexcept
{ msdfgen::destroyFont(font); }

auto GlyphLoader::load_glyph(ssize_t glyph_idx) -> optional<GlyphGeometry>
{
	auto glyph = lib::msdf::GlyphGeometry{};
	auto const scale = 1.0 / lib::harfbuzz::units_per_em(ft_font);
	if (glyph.load(font, scale, msdfgen::GlyphIndex{static_cast<uint>(glyph_idx)})) {
		glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, 3.0, 0);
		glyph.wrapBox(pixels_per_em, 8.0 / pixels_per_em, 1.0);
		return glyph;
	}
	return nullopt;
}

auto add_glyphs(MTSDFAtlas& atlas, span<GlyphGeometry> glyphs) -> vector<GlyphLayout>
{
	atlas.add(glyphs.data(), glyphs.size());

	auto layouts = vector<GlyphLayout>{};
	layouts.reserve(glyphs.size());
	for (auto const& glyph: glyphs) {
		auto const rect = glyph.getBoxRect();
		auto const scale = glyph.getBoxScale();
		auto const translate = glyph.getBoxTranslate();

		layouts.emplace_back(GlyphLayout{
			.atlas_bounds = {
				{static_cast<float>(rect.x), static_cast<float>(rect.y)},
				{static_cast<float>(rect.x + rect.w), static_cast<float>(rect.y + rect.h)},
			},
			.bearing = float2{
				static_cast<float>(translate.x * scale),
				static_cast<float>(rect.h - translate.y * scale),
			},
		});
	}
	return layouts;
}

auto get_atlas_contents(MTSDFAtlas const& atlas) -> AtlasView
{
	auto const& storage = atlas.atlasGenerator().atlasStorage();
	auto bitmap = static_cast<msdfgen::BitmapConstRef<msdf_atlas::byte, 4>>(storage);
	return AtlasView{
		reinterpret_cast<byte const*>(bitmap.pixels),
		boost::extents[bitmap.height][bitmap.width][4],
	};
}

void atlas_to_image(MTSDFAtlas const& atlas, fs::path const& path)
{
	if (!msdf_atlas::saveImage(
		static_cast<msdfgen::BitmapConstRef<msdf_atlas::byte, 4>>(atlas.atlasGenerator().atlasStorage()),
		msdf_atlas::ImageFormat::PNG, path.string().c_str(), msdf_atlas::YDirection::BOTTOM_UP)
	)
		throw runtime_error_fmt("Failed to save font atlas to \"{}\"", path);
}

}

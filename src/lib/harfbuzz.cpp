/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "lib/harfbuzz.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>
#include "preamble.hpp"
#include "utils/assert.hpp"

namespace playnote::lib::harfbuzz {

// Helper functions for error handling

static auto ft_ret_check(int ret) -> int
{
	if (ret != 0) throw runtime_error_fmt("freetype error #{}", ret);
	return ret;
}

void detail::ContextDeleter::operator()(FT_LibraryRec_* ctx) noexcept
{
	FT_Done_FreeType(ctx);
}

void detail::FontDeleter::operator()(Font_t* font) noexcept
{
	hb_font_destroy(font->font);
	FT_Done_Face(font->face);
	delete font;
}

auto init() -> Context
{
	auto* ctx = FT_Library{nullptr};
	ft_ret_check(FT_Init_FreeType(&ctx));
	return Context{ctx};
}

auto create_font(Context& ctx, span<byte const> file) -> Font
{
	auto* face = FT_Face{nullptr};
	ft_ret_check(FT_New_Memory_Face(ctx.get(),
		reinterpret_cast<unsigned char const*>(file.data()), file.size(),
		0, &face));

	ft_ret_check(FT_Set_Char_Size(face, face->units_per_EM << 6, 0, 0, 0));
	auto* font = hb_ft_font_create_referenced(face);
	hb_ft_font_set_load_flags(font, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP);
	hb_ft_font_set_funcs(font);
	return Font{new Font_t{face, font}};
}

auto units_per_em(Font const& font) -> float
{
	return static_cast<float>(font->face->units_per_EM);
}

auto has_glyph(Font const& font, char32_t scalar) -> bool
{
	auto dummy = hb_codepoint_t{};
	return hb_font_get_nominal_glyph(font->font, scalar, &dummy);
}

auto shape(string_view context, string_view run, Font const& font) -> ShapedRun
{
	// Ensure substring relation
	ASSUME(run.data() >= context.data());
	ASSUME(run.data() + run.size() <= context.data() + context.size());

	using Buffer = unique_resource<hb_buffer_t*, decltype([](auto* b) noexcept { hb_buffer_destroy(b); })>;
	auto buffer = Buffer{hb_buffer_create()};
	hb_buffer_add_utf8(buffer.get(),
		context.data(), context.size(),
		run.data() - context.data(), run.size());
	hb_buffer_guess_segment_properties(buffer.get());

	hb_shape(font->font, buffer.get(), nullptr, 0);

	auto glyph_count = 0u;
	auto* infos_raw = hb_buffer_get_glyph_infos(buffer.get(), &glyph_count);
	auto* positions_raw = hb_buffer_get_glyph_positions(buffer.get(), &glyph_count);
	auto infos = span{infos_raw, glyph_count};
	auto positions = span{positions_raw, glyph_count};

	auto result = ShapedRun{};
	result.glyphs.reserve(glyph_count);
	auto const scale = 1.0f / 64.0f;
	auto cursor = float2{0.0f, 0.0f};
	for (auto [info, position]: views::zip(infos, positions)) {
		auto const offset  = float2{position.x_offset  * scale, position.y_offset  * scale};
		auto const advance = float2{position.x_advance * scale, position.y_advance * scale};
		result.glyphs.emplace_back(info.codepoint, cursor + offset);
		cursor += advance;
	}
	result.advance = cursor;
	return result;
}

}

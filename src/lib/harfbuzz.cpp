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
#include FT_MULTIPLE_MASTERS_H
#include <hb.h>
#include <hb-ft.h>
#include "preamble.hpp"
#include "utils/assert.hpp"
#include "io/file.hpp"

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

auto create_font(Context& ctx, shared_ptr<io::ReadFile> file, int weight) -> Font
{
	auto* face = FT_Face{nullptr};
	ft_ret_check(FT_New_Memory_Face(ctx.get(),
		reinterpret_cast<unsigned char const*>(file->contents.data()), file->contents.size(),
		0, &face));

	// Set font metrics
	auto* mm_var = static_cast<FT_MM_Var*>(nullptr);
	ft_ret_check(FT_Get_MM_Var(face, &mm_var));
	ASSUME(mm_var);
	auto coords = vector<FT_Fixed>{};
	coords.reserve(mm_var->num_axis);
	auto axes = span{mm_var->axis, mm_var->num_axis};
	transform(axes, back_inserter(coords), [&](auto const& axis) {
		if (axis.tag == FT_MAKE_TAG('w', 'g', 'h', 't'))
			return clamp(static_cast<FT_Fixed>(weight) << 16, axis.minimum, axis.maximum);
		return axis.def;
	});
	ft_ret_check(FT_Set_Var_Design_Coordinates(face, coords.size(), coords.data()));
	FT_Done_MM_Var(ctx.get(), mm_var);
	ft_ret_check(FT_Set_Char_Size(face, face->units_per_EM << 6, 0, 0, 0));

	auto* font = hb_ft_font_create_referenced(face);
	hb_ft_font_set_load_flags(font, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP);
	hb_ft_font_set_funcs(font);
	return Font{new Font_t{move(file), face, font}};
}

auto has_glyph(Font const& font, char32_t scalar) -> bool
{
	auto dummy = hb_codepoint_t{};
	return hb_font_get_nominal_glyph(font->font, scalar, &dummy);
}

}

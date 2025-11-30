/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "lib/freetype.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include "preamble.hpp"
#include "io/file.hpp"

namespace playnote::lib::freetype {

// Helper functions for error handling

static auto ret_check(int ret) -> int
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
	FT_Done_Face(font->face);
	delete font;
}

auto init() -> Context
{
	auto* ctx = FT_Library{nullptr};
	ret_check(FT_Init_FreeType(&ctx));
	return Context{ctx};
}

auto create_font(Context& ctx, io::ReadFile&& file) -> Font
{
	auto* face = FT_Face{nullptr};
	ret_check(FT_New_Memory_Face(ctx.get(),
		reinterpret_cast<unsigned char const*>(file.contents.data()), file.contents.size(),
		0, &face));
	return Font{new Font_t{move(file), face}};
}

}

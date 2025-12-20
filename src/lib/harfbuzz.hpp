/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "io/file.hpp"

struct FT_LibraryRec_;
struct FT_FaceRec_;
struct hb_font_t;

namespace playnote::lib::harfbuzz {

struct Font_t {
	shared_ptr<io::ReadFile> file;
	FT_FaceRec_* face;
	hb_font_t* font;
};

namespace detail {
struct ContextDeleter {
	static void operator()(FT_LibraryRec_* ctx) noexcept;
};
struct FontDeleter {
	static void operator()(Font_t* font) noexcept;
};
}

using Context = unique_resource<FT_LibraryRec_*, detail::ContextDeleter>;
using Font = unique_resource<Font_t*, detail::FontDeleter>;

// Create a Freetype context, required for font loading.
auto init() -> Context;

// Open a font file for reading.
auto create_font(Context&, shared_ptr<io::ReadFile>) -> Font;

// Return the units-per-em of a font; useful for normalization.
auto units_per_em(Font const&) -> float;

// Return true if a font has a nominal glyph for a Unicode scalar.
auto has_glyph(Font const&, char32_t) -> bool;

struct ShapedRun {
	struct Glyph {
		uint32_t idx;
		float2 offset;
	};
	vector<Glyph> glyphs;
	float2 advance;
};

auto shape(string_view context, string_view run, Font const&) -> ShapedRun;

}

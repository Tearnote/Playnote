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

namespace playnote::lib::freetype {

struct Font_t {
	io::ReadFile file;
	FT_FaceRec_* face;
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

auto init() -> Context;

auto create_font(Context&, io::ReadFile&&) -> Font;

}

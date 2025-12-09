/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"

namespace playnote::lib::icu {

// Detect the most likely encoding of a piece of text. If a list of charsets is provided, then
// only charsets belonging to the list will be considered. Returns nullopt on no match.
// Throws runtime_error on ICU failure.
auto detect_encoding(span<byte const> input, initializer_list<string_view> charsets = {}) -> optional<string>;

// Convert text from the provided charset to UTF-8.
// Throws on ICU error; invalid bytes in the input data however will be decoded without error
// as a replacement character.
auto to_utf8(span<byte const> input, string_view input_charset) -> string;

// Iterate over a UTF-8 string, returning all grapheme clusters in turn. By definition,
// the returned spans might contain one or multiple UTF-8 scalars.
// Throws on ICU error.
auto grapheme_clusters(string_view input) -> generator<string_view>;
auto grapheme_clusters(string&&) -> generator<string_view> = delete; // Prevent dangling views

}

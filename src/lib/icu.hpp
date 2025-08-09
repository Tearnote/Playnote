/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/icu.cppm:
Wrapper for ICU charset handling.
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

}

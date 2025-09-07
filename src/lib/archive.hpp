/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/archive.hpp:
Wrapper for libarchive.
*/

#pragma once
#include "preamble.hpp"

// Forward declarations

struct archive;
struct archive_entry;

namespace playnote::lib::archive {

using Archive = ::archive*;

// Open an archive for reading.
auto open_read(span<byte const>) -> Archive;

// Close an archive previously opened for reading.
void close_read(Archive&&) noexcept;

// Call the provided function on each entry in the archive. The function can optionally call
// read_data() to retrieve the entry's contents. If the function returns false, iteration
// is aborted.
template<callable<bool(string_view)> Func>
void for_each_entry(Archive, Func&&);

// Read the contents of the current entry. To be used from within a for_each_entry() callback.
auto read_data(Archive) -> vector<byte>;

namespace detail {

auto next_entry(Archive) -> optional<string_view>;

}

template<callable<bool(string_view)> Func>
void for_each_entry(Archive archive, Func&& func)
{
	while (true) {
		auto pathname_opt = detail::next_entry(archive);
		if (!pathname_opt) break;
		if (!func(*pathname_opt)) break;
	}
}

}

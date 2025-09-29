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

// Open an archive for writing.
auto open_write(fs::path const&) -> Archive;

// Close an archive previously opened for writing.
void close_write(Archive&&) noexcept;

// Call the provided function on each entry in the archive. The function can optionally call
// read_data() to retrieve the entry's contents. If the function returns false, iteration
// is aborted.
template<callable<bool(string_view)> Func>
void for_each_entry(Archive, Func&&);

// Read the contents of the current entry. To be used from within a for_each_entry() callback.
auto read_data(Archive) -> vector<byte>;

// Write an entry into the archive.
void write_entry(Archive, fs::path const& pathname, span<byte const> data);

// Write an entry into the archive. The provided function is called repeatedly to generate the
// entry's contents, stopping once it returns nullopt.
template<callable<optional<span<byte const>>()> Func>
void write_entry(Archive, fs::path const& pathname, usize total_size, Func&&);

namespace detail {

auto next_entry(Archive) -> optional<string_view>;
void write_header_for(Archive, fs::path const& pathname, usize total_size);
void write_data(Archive, span<byte const> data);

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

template<callable<optional<span<byte const>>()> Func>
void write_entry(Archive archive, fs::path const& pathname, usize total_size, Func&& func)
{
	detail::write_header_for(archive, pathname, total_size);
	while (auto data = func()) detail::write_data(archive, data);
}

}

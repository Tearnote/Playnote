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

namespace detail {
struct ReadArchiveDeleter {
	static void operator()(::archive*) noexcept;
};
struct WriteArchiveDeleter {
	static void operator()(::archive*) noexcept;
};
}

using ReadArchive = unique_resource<::archive*, detail::ReadArchiveDeleter>;
using WriteArchive = unique_resource<::archive*, detail::WriteArchiveDeleter>;

// Open an archive for reading.
auto open_read(span<byte const>) -> ReadArchive;

// Open an archive for writing.
auto open_write(fs::path const&) -> WriteArchive;

// Call the provided function on each entry in the archive. The function can optionally call
// read_data() or read_data_block() to retrieve the entry's contents. If the function returns false,
// iteration is aborted.
template<callable<bool(string_view)> Func>
void for_each_entry(ReadArchive&, Func&&);

// Read the contents of the current entry. To be used from within a for_each_entry() callback.
auto read_data(ReadArchive&) -> vector<byte>;

// Read a block of the current entry's contents. To be used from within a for_each_entry() callback.
// If entry is uncompressed, the block is guaranteed to be the size of the entire entry.  If EOF
// has been reached, returns nullopt. Drops the offset; therefore incompatible with sparse archives.
auto read_data_block(ReadArchive&) -> optional<span<byte const>>;

// Write an entry into the archive.
void write_entry(WriteArchive&, fs::path const& pathname, span<byte const> data);

// Write an entry into the archive. The provided function is called repeatedly to generate the
// entry's contents, stopping once it returns nullopt.
template<callable<optional<span<byte const>>()> Func>
void write_entry(WriteArchive&, fs::path const& pathname, isize total_size, Func&&);

namespace detail {
auto next_entry(ReadArchive&) -> optional<string_view>;
void write_header_for(WriteArchive&, fs::path const& pathname, isize total_size);
void write_data(WriteArchive&, span<byte const> data);
}

template<callable<bool(string_view)> Func>
void for_each_entry(ReadArchive& archive, Func&& func)
{
	while (true) {
		auto pathname_opt = detail::next_entry(archive);
		if (!pathname_opt) break;
		if (!func(*pathname_opt)) break;
	}
}

template<callable<optional<span<byte const>>()> Func>
void write_entry(WriteArchive& archive, fs::path const& pathname, isize total_size, Func&& func)
{
	detail::write_header_for(archive, pathname, total_size);
	while (auto data = func()) detail::write_data(archive, data);
}

}

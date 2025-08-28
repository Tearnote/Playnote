/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/archive.hpp:
Wrapper for libarchive.
*/

#pragma once

#include <archive_entry.h>
#include <archive.h>
#include "preamble.hpp"
#include "logger.hpp"

// Forward declarations

struct archive;
struct archive_entry;

namespace playnote::lib::archive {

using Archive = ::archive*;

// Open an archive for reading.
auto open_read(span<byte const>) -> Archive;

// Close an archive previously opened for reading.
inline void close_read(Archive&&) noexcept;

// Call the provided function on each entry in the archive. The function can optionally call
// read_data() to retrieve the entry's contents. If the function returns false, iteration
// is aborted.
template<callable<bool(string_view)> Func>
void for_each_entry(Archive, Func&&);

// Read the contents of the current entry. To be used from within a for_each_entry() callback.
auto read_data(Archive) -> vector<byte>;

// Helper functions for error handling

inline void ret_check(int ret, optional<Archive> archive = nullopt)
{
	auto message = archive ? archive_error_string(*archive) : "libarchive error";
	if (ret == ARCHIVE_WARN) WARN("{}", message);
	if (ret != ARCHIVE_OK) throw system_error_fmt("{}", message);
}

inline auto open_read(span<byte const> data) -> Archive
{
	auto archive = archive_read_new();
	archive_read_support_format_all(archive);
	archive_read_support_filter_all(archive);
	ret_check(archive_read_open_memory(archive, data.data(), data.size()), archive);
	return archive;
}

inline void close_read(Archive&& archive) noexcept
{
	archive_read_free(archive);
}

template<callable<bool(string_view)> Func>
void for_each_entry(Archive archive, Func&& func)
{
	while (true) {
		auto* entry = static_cast<archive_entry*>(nullptr);
		auto const ret = archive_read_next_header(archive, &entry);
		if (ret == ARCHIVE_EOF) break;
		ret_check(ret, archive);
		if (!func(archive_entry_pathname(entry))) break;
	}
}

inline auto read_data(Archive archive) -> vector<byte>
{
	auto result = vector<byte>{};
	auto* buf = static_cast<byte const*>(nullptr);
	auto size = 0zu;
	auto offset = 0z;
	while (true) {
		auto ret = archive_read_data_block(archive, reinterpret_cast<void const**>(&buf), &size, &offset);
		if (ret != ARCHIVE_EOF) ret_check(ret, archive);
		result.resize(offset + size);
		copy(span{buf, size}, &result[offset]);

		if (ret == ARCHIVE_EOF) break;
	}
	return result;
}

}

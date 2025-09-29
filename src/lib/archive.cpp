/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/archive.cpp:
Implementation file for lib/archive.hpp.
*/

#include "lib/archive.hpp"

#include <archive_entry.h>
#include <archive.h>
#include "preamble.hpp"
#include "logger.hpp"

namespace playnote::lib::archive {

// Helper function for error handling
static void ret_check(int ret, optional<Archive> archive = nullopt)
{
	auto message = archive ? archive_error_string(*archive) : "libarchive error";
	if (ret == ARCHIVE_WARN) WARN("{}", message);
	if (ret != ARCHIVE_OK) throw system_error_fmt("{}", message);
}

auto open_read(span<byte const> data) -> Archive
{
	auto archive = archive_read_new();
	archive_read_support_format_all(archive);
	archive_read_support_filter_all(archive);
	ret_check(archive_read_open_memory(archive, data.data(), data.size()), archive);
	return archive;
}

void close_read(Archive&& archive) noexcept
{
	archive_read_free(archive);
}

auto open_write(fs::path const& path) -> Archive
{
	auto archive = archive_write_new();
	archive_write_set_format_zip(archive);
	archive_write_zip_set_compression_store(archive);
	ret_check(archive_write_open_filename(archive, path.string().c_str()), archive);
	return archive;
}

void close_write(Archive&& archive) noexcept
{
	archive_write_free(archive);
}

auto read_data(Archive archive) -> vector<byte>
{
	auto result = vector<byte>{};
	auto* buf = static_cast<byte const*>(nullptr);
	auto size = 0zu;
	auto offset = 0z;
	while (true) {
		auto ret = archive_read_data_block(archive, reinterpret_cast<void const**>(&buf), &size, &offset);
		if (ret == ARCHIVE_EOF) break;
		ret_check(ret, archive);

		result.resize(offset + size);
		copy(span{buf, size}, &result[offset]);

	}
	return result;
}

void write_entry(Archive archive, fs::path const& pathname, span<byte const> data) {
	detail::write_header_for(archive, pathname, data.size());
	detail::write_data(archive, data);
}

auto detail::next_entry(Archive archive) -> optional<string_view>
{
	auto* entry = static_cast<archive_entry*>(nullptr);
	auto const ret = archive_read_next_header(archive, &entry);
	if (ret == ARCHIVE_EOF) return nullopt;
	ret_check(ret, archive);
	return archive_entry_pathname(entry);
}

void detail::write_header_for(Archive archive, fs::path const& pathname, usize total_size)
{
	auto entry = archive_entry_new();
	archive_entry_set_pathname(entry, pathname.string().c_str());
	archive_entry_set_size(entry, total_size);
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_write_header(archive, entry);
	archive_entry_free(entry);
}

void detail::write_data(Archive archive, span<byte const> data)
{
	archive_write_data(archive, data.data(), data.size());
}

}

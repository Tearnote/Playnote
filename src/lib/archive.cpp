/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "lib/archive.hpp"

#include <archive_entry.h>
#include <archive.h>
#include "preamble.hpp"
#include "utils/logger.hpp"

namespace playnote::lib::archive {

// Helper function for error handling
template<typename T>
requires same_as<T, ReadArchive> || same_as<T, WriteArchive>
static void ret_check(int ret, T& archive = {})
{
	auto message = archive.allocated()? archive_error_string(archive.get()) : "libarchive error";
	if (ret == ARCHIVE_WARN) WARN("{}", message);
	if (ret != ARCHIVE_OK) throw system_error_fmt("{}", message);
}

auto open_read(span<byte const> data) -> ReadArchive
{
	if (data.empty()) throw runtime_error{"Cannot open archive from an empty file"};
	auto archive = archive_read_new();
	archive_read_support_format_all(archive);
	archive_read_support_filter_all(archive);
	auto const ret = archive_read_open_memory(archive, data.data(), data.size());
	auto result = ReadArchive{archive};
	ret_check(ret, result);
	return result;
}

void detail::ReadArchiveDeleter::operator()(::archive* ar) noexcept
{
	archive_read_free(ar);
}

auto open_write(fs::path const& path) -> WriteArchive
{
	auto archive = archive_write_new();
	archive_write_set_format_zip(archive);
	archive_write_zip_set_compression_store(archive);
	auto const ret = archive_write_open_filename(archive, path.string().c_str());
	auto result = WriteArchive{archive};
	ret_check(ret, result);
	return result;
}

void detail::WriteArchiveDeleter::operator()(::archive* ar) noexcept
{
	archive_write_free(ar);
}

auto for_each_entry(ReadArchive& archive) -> generator<string_view>
{
	while (true) {
		auto* entry = static_cast<archive_entry*>(nullptr);
		auto const ret = archive_read_next_header(archive.get(), &entry);
		if (ret == ARCHIVE_EOF) co_return;
		if (archive_entry_filetype(entry) != AE_IFREG) continue;
		ret_check(ret, archive);
		co_yield archive_entry_pathname(entry);
	}
}

auto read_data(ReadArchive& archive) -> vector<byte>
{
	auto result = vector<byte>{};
	auto* buf = static_cast<byte const*>(nullptr);
	auto size = 0zu;
	auto offset = 0z;
	while (true) {
		auto const ret = archive_read_data_block(archive.get(), reinterpret_cast<void const**>(&buf),
			&size, &offset);
		if (ret == ARCHIVE_EOF) break;
		ret_check(ret, archive);

		result.resize(offset + size);
		if (size > 0) copy(span{buf, size}, &result[offset]);

	}
	return result;
}

auto read_data_block(ReadArchive& archive) -> optional<span<byte const>>
{
	auto* buf = static_cast<byte const*>(nullptr);
	auto size = 0zu;
	auto offset = 0z;
	auto const ret = archive_read_data_block(archive.get(), reinterpret_cast<void const**>(&buf),
		&size, &offset);
	if (ret == ARCHIVE_EOF) return nullopt;
	return span{buf, size};
}

void write_entry(WriteArchive& archive, fs::path const& pathname, span<byte const> data) {
	detail::write_header_for(archive, pathname, data.size());
	detail::write_data(archive, data);
}

void detail::write_header_for(WriteArchive& archive, fs::path const& pathname, isize_t total_size)
{
	auto entry = archive_entry_new();
	archive_entry_set_pathname(entry, pathname.string().c_str());
	archive_entry_set_size(entry, total_size);
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_write_header(archive.get(), entry);
	archive_entry_free(entry);
}

void detail::write_data(WriteArchive& archive, span<byte const> data)
{
	archive_write_data(archive.get(), data.data(), data.size());
}

}

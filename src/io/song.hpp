/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

io/song.hpp:
An abstraction for an imported BMS song. Wraps a zip archive with accelerated file lookup.
*/

#pragma once
#include "preamble.hpp"
#include "lib/archive.hpp"
#include "io/file.hpp"

namespace playnote::io {

class Song {
public:
	// Return true if the provided path has a BMS file extension.
	[[nodiscard]] static auto is_bms_file(fs::path const&) -> bool;

	// Create a Song-compatible zip archive from an arbitrary archive. Subfolders inside the archive
	// are handled.
	// Throws runtime_error on failure.
	static void zip_from_archive(fs::path const& src, fs::path const& dst);

	// Create a Song-compatible zip archive from a directory. Directory must directly contain
	// the BMS files.
	// Throws runtime_error on failure.
	static void zip_from_directory(fs::path const& src, fs::path const& dst);

private:
	static constexpr auto BMSExtensions = to_array({".bms", ".bme", ".bml", ".pms"});

	Song() = default; // Use factory methods
	[[nodiscard]] static auto find_prefix(span<byte const> const&) -> fs::path;
};

inline auto Song::is_bms_file(fs::path const& path) -> bool
{
	auto const ext = path.extension().string();
	return find_if(BMSExtensions, [&](auto const& e) { return iequals(e, ext); }) != BMSExtensions.end();
}

inline void Song::zip_from_archive(fs::path const& src, fs::path const& dst)
{
	auto dst_ar = lib::archive::open_write(dst);
	auto wrote_something = false;
	auto src_file = read_file(src);
	auto const prefix = find_prefix(src_file.contents);
	auto src_ar = lib::archive::open_read(src_file.contents);
	lib::archive::for_each_entry(src_ar, [&](string_view pathname) {
		auto data = lib::archive::read_data(src_ar);
		lib::archive::write_entry(dst_ar, fs::relative(pathname, prefix), data);
		wrote_something = true;
		return true;
	});
	if (!wrote_something)
		throw runtime_error_fmt("Failed to create library zip from \"{}\": empty archive", src);
}

inline void Song::zip_from_directory(fs::path const& src, fs::path const& dst)
{
	auto dst_ar = lib::archive::open_write(dst);
	auto wrote_something = false;
	for (auto const& entry: fs::recursive_directory_iterator{src}) {
		if (!entry.is_regular_file()) continue;
		auto const rel_path = fs::relative(entry.path(), src);
		lib::archive::write_entry(dst_ar, rel_path, read_file(entry.path()).contents);
		wrote_something = true;
	}
	if (!wrote_something)
		throw runtime_error_fmt("Failed to create library zip from \"{}\": empty archive", src);
}

inline auto Song::find_prefix(span<byte const> const& archive_data) -> fs::path
{
	auto shortest_prefix = fs::path{};
	auto shortest_prefix_parts = -1zu;
	auto archive = lib::archive::open_read(archive_data);
	lib::archive::for_each_entry(archive, [&](auto pathname) {
		auto const path = fs::path{pathname};
		if (is_bms_file(path)) {
			auto const parts = distance(path.begin(), path.end());
			if (parts < shortest_prefix_parts) {
				shortest_prefix = path.parent_path().string();
				shortest_prefix_parts = parts;
			}
		}
		return true;
	});

	if (shortest_prefix_parts == -1zu)
		throw runtime_error_fmt("No BMS files found in archive");
	return shortest_prefix;
}

}

/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "lib/archive.hpp"
#include "lib/icu.hpp"
#include "io/file.hpp"
#include "preamble/os.hpp"

namespace playnote::io {

// A filesystem location; archive or directory. Provides helpers for discovery of BMS content.
class Source {
public:
	// A reference returned by contents iteration methods.
	class FileReference {
	public:
		// Retrieve path of the file, relative to the source location.
		auto get_path() const -> fs::path const& { return path; }

		// Call to read the contents of the entry. If you're not interested
		// in the entry contents, skip it to iterate much faster.
		auto read() -> span<byte const>;

		// Retrieve a copy of the entry contents. The copy is elided in some cases.
		auto read_owned() -> vector<byte>;

	private:
		friend class Source;
		struct DirEntry {
			fs::directory_entry entry;
			optional<ReadFile> file;
		};
		struct ArchiveEntry {
			lib::archive::ReadArchive& archive;
			optional<vector<byte>> contents;
		};
		fs::path path;
		variant<DirEntry, ArchiveEntry> entry;

		FileReference(fs::path path, DirEntry entry): path{move(path)}, entry{move(entry)} {}
		FileReference(fs::path path, ArchiveEntry entry): path{move(path)}, entry{move(entry)} {}
	};

	// Construct from path. Will throw if the path doesn't contain at least one BMS file inside.
	Source(fs::path const&);

	auto get_path() const -> fs::path const& { return path; }

	auto is_archive() const -> bool { return archive.has_value(); }

	// Return every contained file. Recurses into subfolders.
	auto for_each_file() const -> generator<FileReference>;

private:
	struct ArchiveDetails {
		ReadFile file;
		string encoding; // Text encoding used by filenames
		fs::path prefix; // The shallowest path that leads to a BMS file
	};
	fs::path path;
	optional<ArchiveDetails> archive;
};

inline auto Source::FileReference::read() -> span<byte const>
{
	auto result = span<byte const>{};
	visit(visitor{
		[&](DirEntry& e) {
			if (!e.file) e.file = read_file(e.entry.path());
			result = e.file->contents;
		},
		[&](ArchiveEntry& e) {
			if (!e.contents) e.contents = lib::archive::read_data(e.archive);
			result = *e.contents;
		}
	}, entry);
	return result;
}

inline auto Source::FileReference::read_owned() -> vector<byte>
{
	read(); // Populate .entry
	return visit(visitor{
		[](DirEntry& e) { return vector<byte>{e.file->contents.begin(), e.file->contents.end()}; },
		[](ArchiveEntry& e) { return move(*e.contents); }
	}, entry);
}

inline Source::Source(fs::path const& path):
	path{path}
{
	if (!fs::exists(path)) throw runtime_error_fmt("Path does not exist: {}", path.string());
	if (fs::is_regular_file(path)) {
		archive = ArchiveDetails{
			.file = read_file(path),
		};

		// Detect encoding
		auto ar = lib::archive::open_read(archive->file.contents);
		auto filenames = string{};
		for (auto pathname: lib::archive::for_each_entry(ar)) {
			filenames.append(pathname);
			filenames.append("\n");
		}
		auto encoding = lib::icu::detect_encoding(span{reinterpret_cast<byte const*>(filenames.data()), filenames.size()});
		archive->encoding = encoding? move(*encoding) : "Shift_JIS";

		// Find prefix
		auto shortest_prefix = fs::path{};
		auto shortest_prefix_parts = optional<isize_t>{nullopt};
		ar = lib::archive::open_read(archive->file.contents);
		for (auto pathname: lib::archive::for_each_entry(ar)) {
			auto const pathname_bytes = span{reinterpret_cast<byte const*>(pathname.data()), pathname.size()};
			auto const pathname_utf8 = lib::icu::to_utf8(pathname_bytes, archive->encoding);
			auto const path = fs::path{pathname_utf8};
			if (has_extension(path, BMSExtensions)) {
				auto const parts = distance(path.begin(), path.end());
				if (!shortest_prefix_parts || parts < *shortest_prefix_parts) {
					shortest_prefix = path.parent_path().string();
					shortest_prefix_parts = parts;
				}
			}
		}
		if (!shortest_prefix_parts)
			throw runtime_error_fmt("No BMS files found in archive \"{}\"", path);
		archive->prefix = shortest_prefix;
	}
}

inline auto Source::for_each_file() const -> generator<FileReference> {
	if (archive) {
		auto ar = lib::archive::open_read(archive->file.contents);
		for (auto pathname: lib::archive::for_each_entry(ar)) {
			auto const pathname_bytes = span{reinterpret_cast<byte const*>(pathname.data()), pathname.size()};
			auto const pathname_utf8 = lib::icu::to_utf8(pathname_bytes, archive->encoding);
			auto rel_path = fs::relative(pathname_utf8, archive->prefix);
			if (!rel_path.empty() && *rel_path.begin() == "..") continue;
			co_yield FileReference(rel_path, FileReference::ArchiveEntry{ar});
		}
	} else {
		for (auto const& entry: fs::recursive_directory_iterator{path}) {
			if (!entry.is_regular_file()) continue;
			auto rel_path = fs::relative(entry.path(), path);
			co_yield FileReference(rel_path, FileReference::DirEntry{entry});
		}
	}
}

}

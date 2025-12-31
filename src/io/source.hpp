/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "lib/archive.hpp"
#include "io/file.hpp"

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

}

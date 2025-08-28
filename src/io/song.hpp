/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

io/song.hpp:
Abstraction of a song folder or archive.
*/

#pragma once
#include "preamble.hpp"
#include "lib/archive.hpp"
#include "io/file.hpp"

namespace playnote::io {

class Song {
public:
	// A reference to a file in the song. Can be used to load the file's contents.
	struct FileRef {
		string_view filename;
		Song& song;
		optional<lib::archive::Archive> archive;
		auto load() -> vector<byte>;
	};

	// Open a song folder or archive.
	explicit Song(fs::path domain);

	// Load a specific BMS file from the song.
	// This is potentially slow, as it might need to parse the domain until the file is found.
	[[nodiscard]] auto load_bms(string_view filename) const -> vector<byte>;

	// Execute the provided function for every file in the song. The function is expected to examine
	// each file and load its contents if necessary.
	template<callable<void(FileRef)> Func>
	void for_each_file(Func&&);

private:
	enum class DomainType {
		Folder,
		Archive,
	};
	fs::path domain;
	DomainType type;
};

inline auto Song::FileRef::load() -> vector<byte>
{
	if (song.type == DomainType::Archive) {
		return lib::archive::read_data(*archive);
	} else {
		auto file = read_file(song.domain / filename);
		return vector<byte>{file.contents.begin(), file.contents.end()};
	}
}

inline Song::Song(fs::path domain):
	domain{move(domain)}
{
	if (is_regular_file(this->domain))
		type = DomainType::Archive;
	else
		type = DomainType::Folder;
}

inline auto Song::load_bms(string_view filename) const -> vector<byte>
{
	if (type == DomainType::Archive) {
		auto archive_file = read_file(domain);
		auto archive = lib::archive::open_read(archive_file.contents);

		auto result = vector<byte>{};
		lib::archive::for_each_entry(archive, [&](auto pathname) {
			if (pathname != filename) return true;
			result = lib::archive::read_data(archive);
			return false;
		});

		lib::archive::close_read(move(archive));
		if (result.empty()) throw runtime_error_fmt("BMS file \"{}\" not found in archive", filename);
		return result;
	} else {
		auto file = read_file(domain / filename);
		return vector<byte>{file.contents.begin(), file.contents.end()};
	}
}

template<callable<void(Song::FileRef)> Func>
void Song::for_each_file(Func&& func)
{
	if (type == DomainType::Archive) {
		auto archive_file = read_file(domain);
		auto archive = lib::archive::open_read(archive_file.contents);

		lib::archive::for_each_entry(archive, [&](auto pathname) {
			func(FileRef{
				.filename = pathname,
				.song = *this,
				.archive = archive,
			});
			return true;
		});

		lib::archive::close_read(move(archive));
	} else {
		for (auto const& entry: fs::recursive_directory_iterator{domain} |
			views::filter([](auto const& entry) { return entry.is_regular_file(); })) {
			func(FileRef{
				.filename = fs::relative(entry, domain).string(),
				.song = *this,
			});
		}
	}
}

}

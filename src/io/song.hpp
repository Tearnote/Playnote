/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

io/song.hpp:
Abstraction of a song folder or archive.
*/

#pragma once
#include "preamble.hpp"
#include "io/file.hpp"

namespace playnote::io {

class Song {
public:
	// A reference to a file in the song. Can be used to load the file's contents.
	struct FileRef {
		string_view filename;
		Song& song;
		auto load() -> vector<byte>;
	};

	// Open a song folder or archive.
	explicit Song(fs::path const& domain): domain{domain} {}

	// Load a specific BMS file from the song.
	// This is potentially slow, as it might need to parse the domain until the file is found.
	auto load_bms(fs::path const& path) const -> vector<byte>;

	// Execute the provided function for every file in the song. The function is expected to examine
	// each file and load its contents if necessary.
	template<callable<void(FileRef)> Func>
	void for_each_file(Func&&);

private:
	fs::path domain;
};

template<callable<void(Song::FileRef)> Func>
void Song::for_each_file(Func&& func)
{
	for (auto const& entry: fs::recursive_directory_iterator{domain} |
		views::filter([](auto const& entry) { return entry.is_regular_file(); })) {
		func(FileRef{
			.filename = fs::relative(entry, domain).string(),
			.song = *this,
		});
	}
}

inline auto Song::FileRef::load() -> vector<byte>
{
	auto file = read_file(song.domain / filename);
	return vector<byte>{file.contents.begin(), file.contents.end()};
}

inline auto Song::load_bms(fs::path const& path) const -> vector<byte>
{
	auto file = read_file(domain / path);
	return vector<byte>{file.contents.begin(), file.contents.end()};
}

}

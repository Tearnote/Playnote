/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

io/file.hpp:
Raw file I/O utilities.
*/

#pragma once
#include <fstream>
#include <ios>
#include "preamble.hpp"
#include "lib/mio.hpp"

namespace playnote::io {

// Lists of the various known extensions.

static constexpr auto BMSExtensions = to_array({
	".bms", ".bme", ".bml", ".pms"
});
static constexpr auto AudioExtensions = to_array({
	".wav", ".mp3", ".ogg", ".flac", ".wma", ".m4a", ".opus", ".aac", ".aiff", ".aif"
});
static constexpr auto WastefulAudioExtensions = to_array({
	".wav", ".aiff", ".aif"
});

// A file open for reading. Contents represents the entire length of the file mapped into memory.
// Map is a RAII wrapper ensuring contents are available.
struct ReadFile {
	fs::path path;
	lib::mio::ReadMapping map;
	span<byte const> contents;
};

// A utility that will delete a file at the end of scope, unless disarmed.
class FileDeleter {
public:
	explicit FileDeleter(fs::path path): path{path} {}
	~FileDeleter() { if (!disarmed) fs::remove(path); }
	void disarm() { disarmed = true; }

	FileDeleter(FileDeleter const&) = delete;
	auto operator=(FileDeleter const&) -> FileDeleter& = delete;
	FileDeleter(FileDeleter&&) = delete;
	auto operator=(FileDeleter&&) -> FileDeleter& = delete;

private:
	fs::path path;
	bool disarmed = false;
};

// Open a file for reading.
// Throws runtime_error if the provided path doesn't exist or isn't a regular file, or if mio
// throws system_error.
inline auto read_file(fs::path const& path) -> ReadFile
{
	auto const status = fs::status(path);
	if (!fs::exists(status))
		throw runtime_error_fmt("{} does not exist", path);
	if (!fs::is_regular_file(status))
		throw runtime_error_fmt("{} is not a regular file", path);

	auto file = ReadFile{
		.path = path,
		.map = lib::mio::ReadMapping{path.c_str()},
	};
	file.contents = {file.map.data(), file.map.size()}; // Can't refer to .map in the same initializer
	return file;
}

// Write provided contents to a file, overwriting if it already exists.
inline void write_file(fs::path const& path, span<byte const> contents)
{
	auto file = std::ofstream{};
	file.exceptions(std::ios::failbit | std::ios::badbit);
	file.open(path, std::ios::binary | std::ios::trunc);
	file.write(reinterpret_cast<char const*>(contents.data()), contents.size());
}

}

/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include <fstream>
#include <ios>
#include "preamble.hpp"
#include "lib/mio.hpp"

namespace playnote::io {

// Lists of the various known extensions.

static constexpr auto BMSExtensions = to_array({
	".bms"sv, ".bme"sv, ".bml"sv, ".pms"sv
});
static constexpr auto AudioExtensions = to_array({
	".wav"sv, ".mp3"sv, ".ogg"sv, ".flac"sv, ".wma"sv, ".m4a"sv, ".opus"sv, ".aac"sv, ".aiff"sv, ".aif"sv
});
static constexpr auto WastefulAudioExtensions = to_array({
	".wav"sv, ".aiff"sv, ".aif"sv
});

// Text encodings expected to be used by BMS content.
static constexpr auto KnownEncodings = {"UTF-8"sv, "Shift_JIS"sv, "EUC-KR"sv};

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

// Check if a path has an extension that matches a set. Case-insensitive.
inline auto has_extension(fs::path const& path, span<string_view const> extensions) -> bool
{
	auto const ext = path.extension().string();
	return find_if(extensions, [&](auto const& e) { return iequals(e, ext); }) != extensions.end();
}

}

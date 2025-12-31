/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "lib/mio.hpp"

namespace playnote::io {

// Lists of the various known extensions.

static constexpr auto BMSExtensions = {
	".bms"sv, ".bme"sv, ".bml"sv, ".pms"sv
};
static constexpr auto AudioExtensions = {
	".wav"sv, ".mp3"sv, ".ogg"sv, ".flac"sv, ".wma"sv, ".m4a"sv, ".opus"sv, ".aac"sv, ".aiff"sv, ".aif"sv
};
static constexpr auto WastefulAudioExtensions = {
	".wav"sv, ".aiff"sv, ".aif"sv
};

// Text encodings expected to be used by BMS content.
static constexpr auto KnownEncodings = {
	"UTF-8"sv, "Shift_JIS"sv, "EUC-KR"sv
};

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
auto read_file(fs::path const&) -> ReadFile;

// Write provided contents to a file, overwriting if it already exists.
void write_file(fs::path const&, span<byte const> contents);

// Check if a path has an extension that matches a set. Case-insensitive.
auto has_extension(fs::path const&, span<string_view const> extensions) -> bool;

}

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

// A file open for reading. Contents represents the entire length of the file mapped into memory.
// Map is a RAII wrapper ensuring contents are available.
struct ReadFile {
	fs::path path;
	lib::mio::ReadMapping map;
	span<byte const> contents;
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

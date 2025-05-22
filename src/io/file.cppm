/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

io/file.cppm:
Raw file I/O utilities.
*/

module;
#include <filesystem>
#include <span>
#include "mio/mmap.hpp"

export module playnote.io.file;

import playnote.stx.except;
import playnote.stx.types;

namespace playnote::io {

namespace fs = std::filesystem;
using stx::usize;

// A file open for reading
struct File {
	fs::path path;
	mio::mmap_source map;
	std::span<char const> contents;
};

// Open a file for reading
export auto read_file(fs::path const& path) -> File
{
	auto status = fs::status(path);
	if (!fs::exists(status))
		throw stx::runtime_error_fmt("\"{}\" does not exist", path.c_str());
	if (!fs::is_regular_file(status))
		throw stx::runtime_error_fmt("\"{}\" is not a regular file", path.c_str());

	auto file = File{
		.path = path,
		.map = mio::mmap_source{path.c_str()},
	};
	file.contents = {file.map.data(), file.map.size()}; // Can't refer to .map in the same initializer
	return file;
}

}

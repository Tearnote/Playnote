/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

util/file.cppm:
File I/O utilities.
*/

module;
#include <string_view>
#include <filesystem>
#include <string>
#include <span>
#include "mio/mmap.hpp"

export module playnote.util.file;

import playnote.stx.except;
import playnote.stx.types;

namespace playnote::util {

namespace fs = std::filesystem;
using stx::usize;

// A file open for reading
struct File {
	std::string path;
	mio::mmap_source map;
	std::span<char const> contents;
};

// Open a file for reading
export auto read_file(std::string_view path) -> File
{
	auto status = fs::status(path);
	if (!fs::exists(status))
		throw stx::runtime_error_fmt("\"{}\" does not exist", path);
	if (!fs::is_regular_file(status))
		throw stx::runtime_error_fmt("\"{}\" is not a regular file", path);

	auto file = File{
		.path = std::string{path},
		.map = mio::mmap_source{path},
	};
	file.contents = {file.map.data(), file.map.size()}; // Can't refer to .map in the same initializer, I think?
	return file;
}

}

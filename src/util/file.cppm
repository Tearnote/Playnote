/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

util/file.cppm:
File I/O utilities.
*/

module;
#include <string_view>
#include <filesystem>
#include <utility>
#include <span>
#include "mio/mmap.hpp"

export module playnote.util.file;

import playnote.stx.except;
import playnote.stx.types;

namespace playnote::util {

namespace fs = std::filesystem;
using stx::usize;

// Open file for reading
// Returns the RAII-managed memory mapping and a span of the contents
// The span is only valid as long as the mapping exists
export auto read_file(std::string_view path) -> std::pair<mio::mmap_source, std::span<char const>>
{
	auto status = fs::status(path);
	if (!fs::exists(status))
		throw stx::runtime_error_fmt("\"{}\" does not exist", path);
	if (!fs::is_regular_file(status))
		throw stx::runtime_error_fmt("\"{}\" is not a regular file", path);

	auto map = mio::mmap_source{path};
	return {
		std::piecewise_construct,
		std::make_tuple(std::move(map)),
		std::make_tuple(map.data(), map.size())
	};
}

}

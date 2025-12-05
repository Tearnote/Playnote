/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "io/file.hpp"

#include <fstream>
#include <ios>
#include "preamble.hpp"

namespace playnote::io {

auto read_file(fs::path const& path) -> ReadFile
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

void write_file(fs::path const& path, span<byte const> contents)
{
	auto file = std::ofstream{};
	file.exceptions(std::ios::failbit | std::ios::badbit);
	file.open(path, std::ios::binary | std::ios::trunc);
	file.write(reinterpret_cast<char const*>(contents.data()), contents.size());
}

auto has_extension(fs::path const& path, span<string_view const> extensions) -> bool
{
	auto const ext = path.extension().string();
	return find_if(extensions, [&](auto const& e) { return iequals(e, ext); }) != extensions.end();
}

}

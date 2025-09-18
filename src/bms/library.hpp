/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/library.hpp:
A database of chart information. Holds cached IR blobs and calculated metadata.
*/

#pragma once
#include "preamble.hpp"
#include "logger.hpp"
#include "lib/sqlite.hpp"

namespace playnote::bms {

class Library {
public:
	// Open an existing library, or create an empty one at the provided path.
	explicit Library(fs::path const&);
	~Library() noexcept;

	Library(Library const&) = delete;
	auto operator=(Library const&) -> Library& = delete;
	Library(Library&&) = delete;
	auto operator=(Library&&) -> Library& = delete;

private:
	lib::sqlite::DB db;
};

inline Library::Library(fs::path const& path): db{lib::sqlite::open(path)}
{
	INFO("Opened song library at \"{}\"", path);
}

inline Library::~Library() noexcept
{
	lib::sqlite::close(db);
}

}

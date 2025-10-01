/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

io/song.hpp:
An abstraction for an imported BMS song. Wraps a zip archive with accelerated file lookup.
*/

#pragma once
#include "preamble.hpp"
#include "lib/archive.hpp"
#include "lib/sqlite.hpp"
#include "io/file.hpp"

namespace playnote::io {

class Song {
public:
	// Return true if the provided extension matches a known BMS extension.
	[[nodiscard]] static auto is_bms_ext(string_view) -> bool;

	// Create a Song-compatible zip archive from an arbitrary archive. Subfolders inside the archive
	// are handled.
	// Throws runtime_error on failure.
	static void zip_from_archive(fs::path const& src, fs::path const& dst);

	// Create a Song-compatible zip archive from a directory. Directory must directly contain
	// the BMS files.
	// Throws runtime_error on failure.
	static void zip_from_directory(fs::path const& src, fs::path const& dst);

	// Create a Song from a zip archive. The zip must be Song-compatible.
	// Throws runtime_error on failure.
	static auto from_zip(fs::path const&) -> Song;

	// Call the provided function for each chart of the song.
	template<callable<void(span<byte const>)> Func>
	void for_each_chart(Func&&);

	// Destroy the song and delete the underlying zip file on disk.
	void remove() && noexcept;

private:
	static constexpr auto BMSExtensions = to_array({".bms", ".bme", ".bml", ".pms"});
	static constexpr auto AudioExtensions = to_array({".wav", ".mp3", ".ogg", ".flac", ".wma", ".m4a", ".opus", ".aac", ".aiff", ".aif"});

	enum class FileType {
		Unknown, // 0
		BMS,     // 1
		Audio,   // 2
	};

	// language=SQLite
	static constexpr auto ContentsSchema = to_array({R"(
		CREATE TABLE contents(
			path TEXT NOT NULL,
			type INTEGER NOT NULL,
			ptr BLOB NOT NULL,
			size INTEGER NOT NULL
		)
	)"sv, R"(
		CREATE INDEX contents_path ON contents(path)
	)"sv});
	// language=SQLite
	static constexpr auto InsertContentsQuery = R"(
		INSERT INTO contents(path, type, ptr, size) VALUES (?1, ?2, ?3, ?4)
	)"sv;
	// language=SQLite
	static constexpr auto SelectChartsQuery = R"(
		SELECT ptr, size FROM contents WHERE type = 1
	)"sv;

	ReadFile file;
	lib::sqlite::DB db;
	lib::sqlite::Statement select_charts;

	Song(ReadFile&&, lib::sqlite::DB&&); // Use factory methods
	[[nodiscard]] static auto find_prefix(span<byte const> const&) -> fs::path;
	[[nodiscard]] static auto is_audio_ext(string_view) -> bool;
	[[nodiscard]] static auto type_from_ext(string_view) -> FileType;
};

template<callable<void(span<byte const>)> Func>
void Song::for_each_chart(Func&& func)
{
	lib::sqlite::query(select_charts, [&](void const* ptr, isize size) {
		func(span{static_cast<byte const*>(ptr), static_cast<usize>(size)});
	});
}

inline auto Song::is_bms_ext(string_view ext) -> bool
{
	return find_if(BMSExtensions, [&](auto const& e) { return iequals(e, ext); }) != BMSExtensions.end();
}

inline void Song::zip_from_archive(fs::path const& src, fs::path const& dst)
{
	auto dst_ar = lib::archive::open_write(dst);
	auto wrote_something = false;
	auto src_file = read_file(src);
	auto const prefix = find_prefix(src_file.contents);
	auto src_ar = lib::archive::open_read(src_file.contents);
	lib::archive::for_each_entry(src_ar, [&](string_view pathname) {
		auto data = lib::archive::read_data(src_ar);
		lib::archive::write_entry(dst_ar, fs::relative(pathname, prefix), data);
		wrote_something = true;
		return true;
	});
	if (!wrote_something)
		throw runtime_error_fmt("Failed to create library zip from \"{}\": empty archive", src);
}

inline void Song::zip_from_directory(fs::path const& src, fs::path const& dst)
{
	auto dst_ar = lib::archive::open_write(dst);
	auto wrote_something = false;
	for (auto const& entry: fs::recursive_directory_iterator{src}) {
		if (!entry.is_regular_file()) continue;
		auto const rel_path = fs::relative(entry.path(), src);
		lib::archive::write_entry(dst_ar, rel_path, read_file(entry.path()).contents);
		wrote_something = true;
	}
	if (!wrote_something)
		throw runtime_error_fmt("Failed to create library zip from \"{}\": empty archive", src);
}

inline auto Song::from_zip(fs::path const& path) -> Song
{
	auto file = read_file(path);
	auto archive = lib::archive::open_read(file.contents);
	auto db = lib::sqlite::open(":memory:");
	lib::sqlite::execute(db, ContentsSchema);
	auto contents_insert = lib::sqlite::prepare(db, InsertContentsQuery);

	lib::archive::for_each_entry(archive, [&](auto entry_path_str) {
		auto entry_path = fs::path{entry_path_str};
		auto const ext = entry_path.extension().string();
		auto const data = *lib::archive::read_data_block(archive);
		auto const type = type_from_ext(ext);
		if (type != FileType::Unknown) entry_path.replace_extension();
		lib::sqlite::execute(contents_insert, entry_path.string(), +type, static_cast<void const*>(data.data()), data.size());
		return true;
	});

	auto song = Song(move(file), move(db));
	return song;
}

inline void Song::remove() && noexcept
{
	WARN("No charts found, deleting song");
	fs::remove(file.path);
}

inline Song::Song(ReadFile&& file, lib::sqlite::DB&& db):
	file{move(file)},
	db{move(db)}
{
	select_charts = lib::sqlite::prepare(this->db, SelectChartsQuery);
}

inline auto Song::find_prefix(span<byte const> const& archive_data) -> fs::path
{
	auto shortest_prefix = fs::path{};
	auto shortest_prefix_parts = -1zu;
	auto archive = lib::archive::open_read(archive_data);
	lib::archive::for_each_entry(archive, [&](auto pathname) {
		auto const path = fs::path{pathname};
		if (is_bms_ext(path.extension().string())) {
			auto const parts = distance(path.begin(), path.end());
			if (parts < shortest_prefix_parts) {
				shortest_prefix = path.parent_path().string();
				shortest_prefix_parts = parts;
			}
		}
		return true;
	});

	if (shortest_prefix_parts == -1zu)
		throw runtime_error_fmt("No BMS files found in archive");
	return shortest_prefix;
}

inline auto Song::is_audio_ext(string_view ext) -> bool
{
	return find_if(AudioExtensions, [&](auto const& e) { return iequals(e, ext); }) != AudioExtensions.end();
}

inline auto Song::type_from_ext(string_view ext) -> FileType
{
	if (is_bms_ext(ext)) return FileType::BMS;
	if (is_audio_ext(ext)) return FileType::Audio;
	return FileType::Unknown;
}

}

/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

io/song.hpp:
An abstraction for an imported BMS song. Wraps a zip archive with accelerated file lookup.
*/

#pragma once
#include "preamble.hpp"
#include "utils/task_pool.hpp"
#include "lib/archive.hpp"
#include "lib/openssl.hpp"
#include "lib/sqlite.hpp"
#include "lib/ffmpeg.hpp"
#include "lib/icu.hpp"
#include "dev/audio.hpp"
#include "io/file.hpp"
#include "audio/mixer.hpp"

namespace playnote::io {

// A song archive optimized for file lookup and zero-copy access.
// Once opened the contents are immutable, but only specified methods are thread-safe due to sqlite state.
class Song {
public:
	// Text encodings expected to be used by BMS songs.
	static constexpr auto KnownEncodings = {"UTF-8"sv, "Shift_JIS"sv, "EUC-KR"sv};

	// Return true if the provided extension matches a known BMS extension.
	[[nodiscard]] static auto is_bms_ext(string_view) -> bool;

	// Iterate over the archive, find all BMS files, and checksum them. Then, run the provided
	// function on each checksum. If the function returns false, stop iteration early.
	// Useful for existence checks before creating the proper zip.
	template<callable<bool(lib::openssl::MD5)> Func>
	static void for_each_chart_checksum_in_archive(fs::path const&, Func&&);

	// Iterate over the directory, find all BMS files, and checksum them. Then, run the provided
	// function on each checksum. If the function returns false, stop iteration early.
	// Useful for existence checks before creating the proper zip.
	// Requirement for the path is the same as in zip_from_directory.
	template<callable<bool(lib::openssl::MD5)> Func>
	static void for_each_chart_checksum_in_directory(fs::path const&, Func&&);

	// Heuristically detect the encoding of an arbitrary archive's filenames.
	static auto detect_archive_filename_encoding(fs::path const&) -> string;

	// Create a Song-compatible zip archive from an arbitrary archive. Subfolders inside the archive
	// are handled.
	// Throws runtime_error on failure.
	static void zip_from_archive(fs::path const& src, fs::path const& dst);

	// Create a Song-compatible zip archive from a directory. Directory must directly contain
	// the BMS files.
	// Throws runtime_error on failure.
	static void zip_from_directory(fs::path const& src, fs::path const& dst);

	// Create a Song-compatible zip from a union of an archive and another archive.
	// Base archive is assumed to be in a Song-compatible format, while the extension archive is
	// arbitrary, as for zip_from_archive().
	// Throws runtime_error on failure.
	static void extend_zip_from_archive(fs::path const& base, fs::path const& ext, fs::path const& dst);

	// Create a Song-compatible zip from a union of an archive and a directory.
	// Base archive is assumed to be in a Song-compatible format, and the directory
	// is subject to the same requirements as zip_from_directory().
	// Throws runtime_error on failure.
	static void extend_zip_from_directory(fs::path const& base, fs::path const& ext, fs::path const& dst);

	// Create a Song from a zip archive. The zip must be Song-compatible.
	// Throws runtime_error on failure.
	static auto from_zip(fs::path const&) -> Song;

	// Call the provided function for each chart of the song.
	template<callable<void(string_view, span<byte const>)> Func>
	void for_each_chart(Func&&);

	// Load the requested file.
	auto load_file(string_view filepath) -> span<byte const>;

	// Preload all audio files to an internal cache. This cache will be used in any later load_audio_file() calls.
	// The loads are performed in parallel. Useful when loading multiple charts of the same song.
	void preload_audio_files();

	// Load the requested audio file, decode it, and resample to current device sample rate. Thread-safe.
	auto load_audio_file(string_view filepath) -> vector<dev::Sample>;

	// Destroy the song and delete the underlying zip file on disk.
	void remove() && noexcept;

private:
	static constexpr auto BMSExtensions = to_array({".bms", ".bme", ".bml", ".pms"});
	static constexpr auto AudioExtensions = to_array({".wav", ".mp3", ".ogg", ".flac", ".wma", ".m4a", ".opus", ".aac", ".aiff", ".aif"});
	static constexpr auto WastefulAudioExtensions = to_array({".wav", ".aiff", ".aif"});

	enum class FileType {
		Unknown, // 0
		BMS,     // 1
		Audio,   // 2
	};

	static constexpr auto ContentsSchema = to_array({R"sql(
		CREATE TABLE contents(
			path TEXT NOT NULL COLLATE nocase,
			type INTEGER NOT NULL,
			ptr BLOB NOT NULL,
			size INTEGER NOT NULL
		)
	)sql"sv, R"sql(
		CREATE INDEX contents_path ON contents(path)
	)sql"sv});
	static constexpr auto InsertContentsQuery = R"sql(
		INSERT INTO contents(path, type, ptr, size) VALUES (?1, ?2, ?3, ?4)
	)sql"sv;
	static constexpr auto SelectChartsQuery = R"sql(
		SELECT path, ptr, size FROM contents WHERE type = 1
	)sql"sv;
	static constexpr auto SelectFileQuery = R"sql(
		SELECT ptr, size FROM contents WHERE path = ?1
	)sql"sv;
	static constexpr auto SelectAudioFileQuery = R"sql(
		SELECT ptr, size FROM contents WHERE type = 2 AND path = ?1
	)sql"sv;
	static constexpr auto SelectAudioFilesQuery = R"sql(
		SELECT path, ptr, size FROM contents WHERE type = 2
	)sql"sv;

	ReadFile file;
	lib::sqlite::DB db;
	lib::sqlite::Statement select_charts;
	lib::sqlite::Statement select_file;
	lib::sqlite::Statement select_audio_files;
	unordered_map<string, vector<dev::Sample>, string_hash> audio_cache;

	Song(ReadFile&&, lib::sqlite::DB&&); // Use factory methods
	[[nodiscard]] static auto find_prefix(span<byte const> const&, string_view encoding) -> fs::path;
	[[nodiscard]] static auto is_audio_ext(string_view) -> bool;
	[[nodiscard]] static auto is_wasteful_audio_ext(string_view) -> bool;
	[[nodiscard]] static auto type_from_ext(string_view) -> FileType;
};

template<callable<bool(lib::openssl::MD5)> Func>
void Song::for_each_chart_checksum_in_archive(fs::path const& path, Func&& func)
{
	auto ar_file = read_file(path);
	auto ar = lib::archive::open_read(ar_file.contents);
	lib::archive::for_each_entry(ar, [&](string_view pathname) {
		auto const ext = fs::path{pathname}.extension().string();
		if (!is_bms_ext(ext)) return true;
		auto data = lib::archive::read_data(ar);
		return func(lib::openssl::md5(data));
	});
}

template<callable<bool(lib::openssl::MD5)> Func>
void Song::for_each_chart_checksum_in_directory(fs::path const& path, Func&& func)
{
	for (auto const& entry: fs::directory_iterator{path}) {
		if (!entry.is_regular_file()) continue;
		auto const ext = entry.path().extension().string();
		if (!is_bms_ext(ext)) continue;
		auto data = read_file(entry);
		if (!func(lib::openssl::md5(data.contents))) break;
	}
}

template<callable<void(string_view, span<byte const>)> Func>
void Song::for_each_chart(Func&& func)
{
	lib::sqlite::query(select_charts, [&](string_view path, void const* ptr, isize size) {
		func(path, span{static_cast<byte const*>(ptr), static_cast<usize>(size)});
	});
}

inline auto Song::is_bms_ext(string_view ext) -> bool
{
	return find_if(BMSExtensions, [&](auto const& e) { return iequals(e, ext); }) != BMSExtensions.end();
}

inline auto Song::detect_archive_filename_encoding(fs::path const& path) -> string
{
	auto ar_file = read_file(path);
	auto ar = lib::archive::open_read(ar_file.contents);
	auto filenames = string{};
	lib::archive::for_each_entry(ar, [&](string_view pathname) {
		filenames.append(pathname);
		filenames.append("\n");
		return true;
	});
	auto encoding = lib::icu::detect_encoding(span{reinterpret_cast<byte const*>(filenames.data()), filenames.size()});
	return encoding? *encoding : "Shift_JIS";
}

inline void Song::zip_from_archive(fs::path const& src, fs::path const& dst)
{
	auto const encoding = detect_archive_filename_encoding(src);
	auto dst_ar = lib::archive::open_write(dst);
	auto wrote_something = false;
	auto src_file = read_file(src);
	auto const prefix = find_prefix(src_file.contents, encoding);
	auto src_ar = lib::archive::open_read(src_file.contents);
	lib::archive::for_each_entry(src_ar, [&](string_view pathname) {
		auto const pathname_bytes = span{reinterpret_cast<byte const*>(pathname.data()), pathname.size()};
		auto const pathname_utf8 = lib::icu::to_utf8(pathname_bytes, encoding);
		auto data = lib::archive::read_data(src_ar);
		auto rel_path = fs::relative(pathname_utf8, prefix);
		if (!rel_path.empty() && *rel_path.begin() == "..") return true;

		auto const ext = rel_path.extension().string();
		if (is_wasteful_audio_ext(ext)) {
			auto const sampling_rate = globals::mixer->get_audio().get_sampling_rate();
			data = lib::ffmpeg::encode_as_ogg(lib::ffmpeg::decode_and_resample_file_buffer(data, sampling_rate), sampling_rate);
			rel_path.replace_extension(".ogg");
		}
		lib::archive::write_entry(dst_ar, rel_path, data);
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
		auto rel_path = fs::relative(entry.path(), src);
		auto file = read_file(entry.path());

		auto converted = optional<vector<byte>>{};
		auto const ext = rel_path.extension().string();
		if (is_wasteful_audio_ext(ext)) {
			auto const sampling_rate = globals::mixer->get_audio().get_sampling_rate();
			converted = lib::ffmpeg::encode_as_ogg(lib::ffmpeg::decode_and_resample_file_buffer(file.contents, sampling_rate), sampling_rate);
			rel_path.replace_extension(".ogg");
		}
		lib::archive::write_entry(dst_ar, rel_path, converted? *converted : file.contents);
		wrote_something = true;
	}
	if (!wrote_something)
		throw runtime_error_fmt("Failed to create library zip from \"{}\": empty archive", src);
}

inline void Song::extend_zip_from_archive(fs::path const& base, fs::path const& ext, fs::path const& dst)
{
	auto dst_ar = lib::archive::open_write(dst);
	auto written_paths = unordered_set<string>{};

	// Copy over contents of base
	auto src_file = read_file(base);
	auto src_ar = lib::archive::open_read(src_file.contents);
	lib::archive::for_each_entry(src_ar, [&](string_view pathname) {
		auto data = lib::archive::read_data(src_ar);
		lib::archive::write_entry(dst_ar, pathname, data);
		written_paths.emplace(pathname);
		return true;
	});

	// Append missing files from extension
	auto const encoding = detect_archive_filename_encoding(ext);
	auto ext_file = read_file(ext);
	auto const prefix = find_prefix(ext_file.contents, encoding);
	auto ext_ar = lib::archive::open_read(ext_file.contents);
	lib::archive::for_each_entry(ext_ar, [&](string_view pathname) {
		auto const pathname_bytes = span{reinterpret_cast<byte const*>(pathname.data()), pathname.size()};
		auto const pathname_utf8 = lib::icu::to_utf8(pathname_bytes, encoding);
		auto rel_path = fs::relative(pathname_utf8, prefix);
		if (!rel_path.empty() && *rel_path.begin() == "..") return true;
		if (written_paths.contains(rel_path.string())) return true;
		auto data = lib::archive::read_data(ext_ar);

		auto const ext = rel_path.extension().string();
		if (is_wasteful_audio_ext(ext)) {
			auto const sampling_rate = globals::mixer->get_audio().get_sampling_rate();
			data = lib::ffmpeg::encode_as_ogg(lib::ffmpeg::decode_and_resample_file_buffer(data, sampling_rate), sampling_rate);
			rel_path.replace_extension(".ogg");
		}
		lib::archive::write_entry(dst_ar, rel_path, data);
		return true;
	});
}

inline void Song::extend_zip_from_directory(fs::path const& base, fs::path const& ext, fs::path const& dst)
{
	auto dst_ar = lib::archive::open_write(dst);
	auto written_paths = unordered_set<string>{};

	// Copy over contents of base
	auto src_file = read_file(base);
	auto src_ar = lib::archive::open_read(src_file.contents);
	lib::archive::for_each_entry(src_ar, [&](string_view pathname) {
		auto data = lib::archive::read_data(src_ar);
		lib::archive::write_entry(dst_ar, pathname, data);
		written_paths.emplace(pathname);
		return true;
	});

	// Append missing files from extension
	for (auto const& entry: fs::recursive_directory_iterator{ext}) {
		if (!entry.is_regular_file()) continue;
		auto rel_path = fs::relative(entry.path(), ext);
		if (written_paths.contains(rel_path.string())) continue;
		auto file = read_file(entry.path());

		auto converted = optional<vector<byte>>{};
		auto const ext = rel_path.extension().string();
		if (is_wasteful_audio_ext(ext)) {
			auto const sampling_rate = globals::mixer->get_audio().get_sampling_rate();
			converted = lib::ffmpeg::encode_as_ogg(lib::ffmpeg::decode_and_resample_file_buffer(file.contents, sampling_rate), sampling_rate);
			rel_path.replace_extension(".ogg");
		}
		lib::archive::write_entry(dst_ar, rel_path, converted? *converted : file.contents);
	}
}

inline auto Song::from_zip(fs::path const& path) -> Song
{
	auto file = read_file(path);
	auto archive = lib::archive::open_read(file.contents);
	auto db = lib::sqlite::open(":memory:");
	lib::sqlite::execute(db, ContentsSchema);
	auto insert_contents = lib::sqlite::prepare(db, InsertContentsQuery);

	lib::archive::for_each_entry(archive, [&](auto entry_path_str) {
		auto entry_path = fs::path{entry_path_str};
		auto const ext = entry_path.extension().string();
		auto const data = lib::archive::read_data_block(archive);
		if (!data) return true;
		auto const type = type_from_ext(ext);
		if (type == FileType::Audio) entry_path.replace_extension();
		lib::sqlite::execute(insert_contents, entry_path.string(), +type, static_cast<void const*>(data->data()), data->size());
		return true;
	});

	auto song = Song(move(file), move(db));
	return song;
}

inline auto Song::load_file(string_view filepath) -> span<byte const>
{
	auto file = span<byte const>{};
	lib::sqlite::query(select_file, [&](void const* ptr, isize size) {
		file = span{static_cast<byte const*>(ptr), static_cast<usize>(size)};
		return false;
	}, filepath);
	if (!file.data())
		throw runtime_error_fmt("File \"{}\" doesn't exist within the song archive", filepath);
	return file;
}

inline void Song::preload_audio_files()
{
	auto tasks = vector<task<vector<dev::Sample>>>{};
	auto paths = vector<string>{};
	lib::sqlite::query(select_audio_files, [&](string_view filepath, void const* ptr, isize size) {
		// Normally the db collation handles case-insensitive lookup for us, but we need to do it manually for the cache
		auto filepath_low = string{filepath};
		to_lower(filepath_low);
		auto file = span{static_cast<byte const*>(ptr), static_cast<usize>(size)};
		tasks.emplace_back(schedule_task([](span<byte const> file) -> task<vector<dev::Sample>> {
			co_return lib::ffmpeg::decode_and_resample_file_buffer(file, globals::mixer->get_audio().get_sampling_rate());
		}(file)));
		paths.emplace_back(move(filepath_low));
	});

	auto results = sync_wait(when_all(move(tasks)));
	for (auto [result, path]: views::zip(results, paths))
		audio_cache.emplace(path, move(result.return_value()));
}

inline auto Song::load_audio_file(string_view filepath) -> vector<dev::Sample>
{
	if (!audio_cache.empty()) {
		auto filepath_low = string{filepath};
		to_lower(filepath_low);
		auto it = audio_cache.find(filepath_low);
		if (it != audio_cache.end())
			return it->second;
	}

	auto file = span<byte const>{};
	auto select_audio_file = lib::sqlite::prepare(this->db, SelectAudioFileQuery);
	lib::sqlite::query(select_audio_file, [&](void const* ptr, isize size) {
		file = span{static_cast<byte const*>(ptr), static_cast<usize>(size)};
		return false;
	}, filepath);
	if (!file.data())
		throw runtime_error_fmt("Audio file \"{}\" doesn't exist within the song archive", filepath);
	return lib::ffmpeg::decode_and_resample_file_buffer(file, globals::mixer->get_audio().get_sampling_rate());
}

inline void Song::remove() && noexcept
{
	fs::remove(file.path);
}

inline Song::Song(ReadFile&& file, lib::sqlite::DB&& db):
	file{move(file)},
	db{move(db)}
{
	select_charts = lib::sqlite::prepare(this->db, SelectChartsQuery);
	select_file = lib::sqlite::prepare(this->db, SelectFileQuery);
	select_audio_files = lib::sqlite::prepare(this->db, SelectAudioFilesQuery);
}

inline auto Song::find_prefix(span<byte const> const& archive_data, string_view encoding) -> fs::path
{
	auto shortest_prefix = fs::path{};
	auto shortest_prefix_parts = optional<isize>{nullopt};
	auto archive = lib::archive::open_read(archive_data);
	lib::archive::for_each_entry(archive, [&](auto pathname) {
		auto const pathname_bytes = span{reinterpret_cast<byte const*>(pathname.data()), pathname.size()};
		auto const pathname_utf8 = lib::icu::to_utf8(pathname_bytes, encoding);
		auto const path = fs::path{pathname_utf8};
		if (is_bms_ext(path.extension().string())) {
			auto const parts = distance(path.begin(), path.end());
			if (!shortest_prefix_parts || parts < *shortest_prefix_parts) {
				shortest_prefix = path.parent_path().string();
				shortest_prefix_parts = parts;
			}
		}
		return true;
	});

	if (!shortest_prefix_parts)
		throw runtime_error_fmt("No BMS files found in archive");
	return shortest_prefix;
}

inline auto Song::is_audio_ext(string_view ext) -> bool
{
	return find_if(AudioExtensions, [&](auto const& e) { return iequals(e, ext); }) != AudioExtensions.end();
}

inline auto Song::is_wasteful_audio_ext(string_view ext) -> bool
{
	return find_if(WastefulAudioExtensions, [&](auto const& e) { return iequals(e, ext); }) != WastefulAudioExtensions.end();
}

inline auto Song::type_from_ext(string_view ext) -> FileType
{
	if (is_bms_ext(ext)) return FileType::BMS;
	if (is_audio_ext(ext)) return FileType::Audio;
	return FileType::Unknown;
}

}

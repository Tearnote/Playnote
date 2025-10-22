/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

io/song.hpp:
The disk portion of an imported BMS song; a zip archive with STORE compression and no leading directory.
*/

#pragma once
#include "preamble.hpp"
#include "utils/task_pool.hpp"
#include "lib/archive.hpp"
#include "lib/sqlite.hpp"
#include "lib/ffmpeg.hpp"
#include "dev/audio.hpp"
#include "io/source.hpp"
#include "io/file.hpp"
#include "audio/mixer.hpp"

namespace playnote::io {

// An archive optimized for file lookup and zero-copy access. Once opened, the contents are immutable.
class Song {
public:
	// Create from an existing songzip.
	explicit Song(Logger::Category, ReadFile&&);

	// Convert from a Source.
	static auto from_source(Logger::Category, Source const&, fs::path const& dst) -> Song;

	// Convert from a Source, using an existing songzip as base.
	static auto from_source_append(Logger::Category, ReadFile&& src, Source const& ext, fs::path const& dst) -> Song;

	// Call the provided function for each chart of the song.
	template<callable<void(string_view, span<byte const>)> Func>
	void for_each_chart(Func&&);

	// Load the requested file.
	auto load_file(string_view filepath) -> span<byte const>;

	// Preload all audio files to an internal cache. This cache will be used in any later load_audio_file() calls.
	// The loads are performed in parallel. Useful when loading multiple charts of the same song.
	auto preload_audio_files() -> task<>;

	// Load the requested audio file, decode it, and resample to current device sample rate.
	auto load_audio_file(string_view filepath) -> vector<dev::Sample>;

	// Destroy the song and delete the underlying songzip from disk.
	void remove() && noexcept;

private:
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

	Logger::Category cat;
	ReadFile file;
	lib::sqlite::DB db;
	lib::sqlite::Statement select_charts;
	lib::sqlite::Statement select_file;
	lib::sqlite::Statement select_audio_files;
	unordered_map<string, vector<dev::Sample>, string_hash> audio_cache;

	static void optimize_file(Logger::Category, fs::path& path, span<byte const>& data);
	[[nodiscard]] static auto type_from_path(fs::path const&) -> FileType;
};

inline Song::Song(Logger::Category cat, ReadFile&& file):
	cat{cat},
	file{move(file)},
	db{lib::sqlite::open(":memory:")}
{
	lib::sqlite::execute(db, ContentsSchema);
	select_charts = lib::sqlite::prepare(db, SelectChartsQuery);
	select_file = lib::sqlite::prepare(db, SelectFileQuery);
	select_audio_files = lib::sqlite::prepare(db, SelectAudioFilesQuery);
	auto insert_contents = lib::sqlite::prepare(db, InsertContentsQuery);

	auto archive = lib::archive::open_read(this->file.contents);
	lib::archive::for_each_entry(archive, [&](auto filepath) {
		auto const data = lib::archive::read_data_block(archive);
		if (!data) return true;
		auto path = fs::path{filepath};
		auto const type = type_from_path(path);
		if (has_extension(path, AudioExtensions)) path.replace_extension();
		lib::sqlite::execute(insert_contents, path.string(), +type, static_cast<void const*>(data->data()), data->size());
		return true;
	});
}

inline auto Song::from_source(Logger::Category cat, Source const& src, fs::path const& dst) -> Song
{
	auto ar = lib::archive::open_write(dst);
	auto wrote_something = false;
	src.for_each_file([&](auto ref) {
		auto data = ref.read();
		auto path = ref.get_path();
		optimize_file(cat, path, data);
		lib::archive::write_entry(ar, path, data);
		wrote_something = true;
		return true;
	});

	if (!wrote_something)
		throw runtime_error_fmt("Failed to create library zip from \"{}\": empty archive", src.get_path());
	ar.reset(); // Finalize archive
	return Song{cat, read_file(dst)};
}

inline auto Song::from_source_append(Logger::Category cat, ReadFile&& src, Source const& ext, fs::path const& dst) -> Song
{
	auto ar = lib::archive::open_write(dst);
	auto written_paths = unordered_set<string>{};

	// Copy over contents of base
	// (We can't just copy the file, because libarchive doesn't support append)
	auto src_ar = lib::archive::open_read(src.contents);
	lib::archive::for_each_entry(src_ar, [&](string_view pathname) {
		auto data = lib::archive::read_data(src_ar);
		lib::archive::write_entry(ar, pathname, data);
		written_paths.emplace(pathname);
		return true;
	});

	// Append missing files
	ext.for_each_file([&](auto entry) {
		auto path = entry.get_path();
		if (written_paths.contains(path.string())) return true;
		auto data = entry.read();
		optimize_file(cat, path, data);
		lib::archive::write_entry(ar, path, data);
		return true;
	});

	ar.reset(); // Finalize archive
	return Song{cat, read_file(dst)};
}

template<callable<void(string_view, span<byte const>)> Func>
void Song::for_each_chart(Func&& func)
{
	lib::sqlite::query(select_charts, [&](string_view path, void const* ptr, isize size) {
		func(path, span{static_cast<byte const*>(ptr), static_cast<usize>(size)});
	});
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

inline auto Song::preload_audio_files() -> task<>
{
	auto tasks = vector<task<vector<dev::Sample>>>{};
	auto paths = vector<string>{};
	lib::sqlite::query(select_audio_files, [&](string_view filepath, void const* ptr, isize size) {
		// Normally the db collation handles case-insensitive lookup for us, but we need to do it manually for the cache
		auto filepath_low = string{filepath};
		to_lower(filepath_low);
		auto file = span{static_cast<byte const*>(ptr), static_cast<usize>(size)};
		tasks.emplace_back(schedule_task([](Logger::Category cat, span<byte const> file) -> task<vector<dev::Sample>> {
			lib::ffmpeg::set_thread_log_category(cat);
			co_return lib::ffmpeg::decode_and_resample_file_buffer(file, globals::mixer->get_audio().get_sampling_rate());
		}(cat, file)));
		paths.emplace_back(move(filepath_low));
	});

	auto results = co_await when_all(move(tasks));
	for (auto [result, path]: views::zip(results, paths)) {
		try {
			audio_cache.emplace(path, move(result.return_value()));
		} catch (exception const& e) {
			WARN_AS(cat, "Failed to preload \"{}\": {}", path, e.what());
		}
	}
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
	lib::ffmpeg::set_thread_log_category(cat);
	return lib::ffmpeg::decode_and_resample_file_buffer(file, globals::mixer->get_audio().get_sampling_rate());
}

inline void Song::remove() && noexcept
{
	fs::remove(file.path);
}

inline void Song::optimize_file(Logger::Category cat, fs::path& path, span<byte const>& data)
try {
	if (has_extension(path, WastefulAudioExtensions)) {
		lib::ffmpeg::set_thread_log_category(cat);
		auto const sampling_rate = globals::mixer->get_audio().get_sampling_rate();
		data = lib::ffmpeg::encode_as_ogg(lib::ffmpeg::decode_and_resample_file_buffer(data, sampling_rate), sampling_rate);
		path.replace_extension(".ogg");
	}
} catch (exception const& e) {
	WARN("Failed to optimize file \"{}\": {}", path, e.what());
}

inline auto Song::type_from_path(fs::path const& path) -> FileType
{
	if (has_extension(path, BMSExtensions)) return FileType::BMS;
	if (has_extension(path, AudioExtensions)) return FileType::Audio;
	return FileType::Unknown;
}

}

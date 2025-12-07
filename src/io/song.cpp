/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "io/song.hpp"

#include "preamble.hpp"
#include "utils/task_pool.hpp"
#include "lib/archive.hpp"
#include "lib/ffmpeg.hpp"

namespace playnote::io {

static auto optimize_audio(Logger::Category cat, fs::path path, vector<byte> data) -> task<pair<fs::path, vector<byte>>>
{
	lib::ffmpeg::set_thread_log_category(cat);
	data = lib::ffmpeg::encode_as_ogg(lib::ffmpeg::decode_and_resample_file_buffer(data, 48000), 48000);
	path.replace_extension(".ogg");
	co_return make_pair(move(path), move(data));
}

template<callable<bool(fs::path const&)> Func>
auto optimize_files(Logger::Category cat, unique_ptr<thread_pool>& pool, Source const& src,
	Func&& filter) -> task<unordered_map<fs::path, pair<fs::path, vector<byte>>>>
{
	// when_all requires an ordered container
	auto optimize_tasks = vector<task<pair<fs::path, vector<byte>>>>{};
	auto optimized_paths = vector<fs::path>{};
	for (auto&& ref: src.for_each_file()) {
		auto path = ref.get_path();
		if (!filter(path)) continue;
		if (!has_extension(path, WastefulAudioExtensions)) continue;
		auto data = ref.read_owned();
		optimized_paths.emplace_back(path);
		optimize_tasks.emplace_back(schedule_task_on(pool, optimize_audio(cat, move(path), move(data))));
	}
	auto optimize_results = co_await when_all(move(optimize_tasks));

	// Convert results to a hashmap for faster lookup
	auto optimized_files = unordered_map<fs::path, pair<fs::path, vector<byte>>>{};
	for (auto [result, path]: views::zip(optimize_results, optimized_paths)) {
		try {
			auto [opt_path, opt_data] = result.return_value();
			optimized_files.emplace(move(path), make_pair(move(opt_path), move(opt_data)));
		} catch (exception const& e) {
			WARN_AS(cat, "Failed to optimize \"{}\": {}", path, e.what());
		}
	}
	co_return optimized_files;
}

enum class FileType {
	Unknown, // 0
	BMS,     // 1
	Audio,   // 2
};

static auto type_from_path(fs::path const& path) -> FileType
{
	if (has_extension(path, BMSExtensions)) return FileType::BMS;
	if (has_extension(path, AudioExtensions)) return FileType::Audio;
	return FileType::Unknown;
}

Song::Song(Logger::Category cat, ReadFile&& file):
	cat{cat},
	file{move(file)},
	db{lib::sqlite::open(":memory:")}
{
	lib::sqlite::execute(db, ContentsSchema);
	select_charts = lib::sqlite::prepare<SelectCharts>(db);
	select_file = lib::sqlite::prepare<SelectFile>(db);
	select_audio_files = lib::sqlite::prepare<SelectAudioFiles>(db);
	auto insert_contents = lib::sqlite::prepare<InsertContents>(db);

	auto archive = lib::archive::open_read(this->file.contents);
	for (auto filepath: lib::archive::for_each_entry(archive)) {
		auto const data = lib::archive::read_data_block(archive);
		if (!data) continue;
		auto path = fs::path{filepath};
		auto const type = type_from_path(path);
		if (has_extension(path, AudioExtensions)) path.replace_extension();
		lib::sqlite::execute(insert_contents, path.string(), +type, static_cast<void const*>(data->data()), data->size());
	}
}

auto Song::from_source(Logger::Category cat, unique_ptr<thread_pool>& pool,
	Source const& src, fs::path const& dst) -> task<Song>
{
	auto ar = lib::archive::open_write(dst);
	auto optimized_files = co_await optimize_files(cat, pool, src, [](auto const&) { return true; });

	auto wrote_something = false;
	for (auto&& ref: src.for_each_file()) {
		auto path = ref.get_path();
		auto optimized = optimized_files.find(path);
		if (optimized != optimized_files.end()) {
			auto [opt_path, opt_data] = optimized->second;
			lib::archive::write_entry(ar, opt_path, opt_data);
			wrote_something = true;
			continue;
		}
		auto data = ref.read();
		lib::archive::write_entry(ar, path, data);
		wrote_something = true;
	}

	if (!wrote_something)
		throw runtime_error_fmt("Failed to create library zip from \"{}\": empty archive", src.get_path());
	ar.reset(); // Finalize archive
	co_return Song{cat, read_file(dst)};
}

auto Song::from_source_append(Logger::Category cat, unique_ptr<thread_pool>& pool,
	ReadFile&& src, Source const& ext, fs::path const& dst) -> task<Song>
{
	auto ar = lib::archive::open_write(dst);
	auto written_paths = unordered_set<string>{};

	// Copy over contents of base
	// (We can't just copy the file, because libarchive doesn't support append)
	auto src_ar = lib::archive::open_read(src.contents);
	for (auto pathname: lib::archive::for_each_entry(src_ar)) {
		auto data = lib::archive::read_data(src_ar);
		lib::archive::write_entry(ar, pathname, data);
		written_paths.emplace(pathname);
	}

	auto optimized_files = co_await optimize_files(cat, pool, ext, [&](auto const& path) {
		return !written_paths.contains(path.string());
	});

	// Append missing files
	for (auto&& ref: ext.for_each_file()) {
		auto path = ref.get_path();
		if (written_paths.contains(path.string())) continue;
		auto optimized = optimized_files.find(path);
		if (optimized != optimized_files.end()) {
			auto [opt_path, opt_data] = optimized->second;
			lib::archive::write_entry(ar, opt_path, opt_data);
			continue;
		}
		auto data = ref.read();
		lib::archive::write_entry(ar, path, data);
	}

	ar.reset(); // Finalize archive
	co_return Song{cat, read_file(dst)};
}

auto Song::for_each_chart() -> generator<tuple<string_view, span<byte const>>>
{
	for (auto [path, ptr, size]: lib::sqlite::query(select_charts))
		co_yield {path, span{static_cast<byte const*>(ptr), static_cast<size_t>(size)}};
}

auto Song::load_file(string_view filepath) -> span<byte const>
{
	auto file = span<byte const>{};
	for (auto [ptr, size]: lib::sqlite::query(select_file, filepath)) {
		file = span{static_cast<byte const*>(ptr), static_cast<size_t>(size)};
		break;
	}
	if (!file.data())
		throw runtime_error_fmt("File \"{}\" doesn't exist within the song archive", filepath);
	return file;
}

auto Song::preload_audio_files(unique_ptr<thread_pool>& pool, int sampling_rate) -> task<>
{
	auto tasks = vector<task<vector<dev::Sample>>>{};
	auto paths = vector<string>{};
	for (auto [filepath, ptr, size]: lib::sqlite::query(select_audio_files)) {
		// Normally the db collation handles case-insensitive lookup for us, but we need to do it manually for the cache
		auto filepath_low = string{filepath};
		to_lower(filepath_low);
		auto file = span{static_cast<byte const*>(ptr), static_cast<size_t>(size)};
		tasks.emplace_back(schedule_task_on(pool, [](Logger::Category cat, span<byte const> file, ssize_t sampling_rate) -> task<vector<dev::Sample>> {
			lib::ffmpeg::set_thread_log_category(cat);
			co_return lib::ffmpeg::decode_and_resample_file_buffer(file, sampling_rate);
		}(cat, file, sampling_rate)));
		paths.emplace_back(move(filepath_low));
	}

	auto results = co_await when_all(move(tasks));
	for (auto [result, path]: views::zip(results, paths)) {
		try {
			audio_cache.emplace(path, move(result.return_value()));
		} catch (exception const& e) {
			WARN_AS(cat, "Failed to preload \"{}\": {}", path, e.what());
		}
	}
}

auto Song::load_audio_file(string_view filepath, int sampling_rate) -> vector<dev::Sample>
{
	if (!audio_cache.empty()) {
		auto filepath_low = string{filepath};
		to_lower(filepath_low);
		auto it = audio_cache.find(filepath_low);
		if (it != audio_cache.end())
			return it->second;
	}

	auto file = span<byte const>{};
	auto select_audio_file = lib::sqlite::prepare<SelectAudioFile>(this->db);
	for (auto [ptr, size]: lib::sqlite::query(select_audio_file, filepath)) {
		file = span{static_cast<byte const*>(ptr), static_cast<size_t>(size)};
		break;
	}
	if (!file.data())
		throw runtime_error_fmt("Audio file \"{}\" doesn't exist within the song archive", filepath);
	lib::ffmpeg::set_thread_log_category(cat);
	return lib::ffmpeg::decode_and_resample_file_buffer(file, sampling_rate);
}

void Song::remove() && noexcept
{
	fs::remove(file.path);
}

}

/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "bms/library.hpp"

#include "preamble.hpp"
#include "utils/task_pool.hpp"
#include "utils/config.hpp"
#include "lib/openssl.hpp"
#include "lib/ffmpeg.hpp"
#include "lib/bits.hpp"
#include "lib/zstd.hpp"
#include "io/source.hpp"
#include "io/file.hpp"
#include "audio/mixer.hpp"
#include "bms/builder.hpp"

namespace playnote::bms {

Library::Library(Logger::Category cat, unique_ptr<thread_pool>& pool, fs::path const& path):
	cat{cat},
	pool{pool},
	db{lib::sqlite::open(path)},
	import_tasks{pool}
{
	lib::sqlite::execute(db, SongsSchema);
	lib::sqlite::execute(db, ChartsSchema);
	lib::sqlite::execute(db, ChartDensitiesSchema);
	lib::sqlite::execute(db, ChartImportLogsSchema);
	lib::sqlite::execute(db, ChartPreviewsSchema);
	fs::create_directory(LibraryPath);
	INFO_AS(cat, "Opened song library at \"{}\"", path);
}

Library::~Library() noexcept
{ stopping.store(true); }

void Library::import(fs::path const& path)
{ import_tasks.start(import_many(path)); }

auto Library::list_charts() -> task<vector<ChartEntry>>
{
	auto chart_listing = lib::sqlite::prepare<ChartListing>(db);
	auto result = vector<ChartEntry>{};
	for (auto [md5, title, playstyle, difficulty]: lib::sqlite::query(chart_listing)) {
		auto entry = ChartEntry{};
		copy(md5, entry.md5.begin());
		auto const difficulty_str = enum_name(static_cast<Difficulty>(difficulty));
		auto const playstyle_str = enum_name(static_cast<Playstyle>(playstyle));
		entry.title = format("{} [{}] [{}]##{}", title, difficulty_str, playstyle_str.substr(1), lib::openssl::md5_to_hex(entry.md5));
		result.emplace_back(move(entry));
	}
	dirty.store(false);
	co_return result;
}

void Library::reset_import_stats()
{
	import_stats.songs_processed.store(0);
	import_stats.songs_total.store(0);
	import_stats.songs_failed.store(0);
	import_stats.charts_added.store(0);
	import_stats.charts_skipped.store(0);
	import_stats.charts_failed.store(0);
}

auto Library::load_chart(unique_ptr<thread_pool>& pool, MD5 md5) -> task<shared_ptr<Chart const>>
{
	auto cache = optional<Metadata>{nullopt};
	auto song_path = fs::path{};
	auto chart_path = string{};
	auto select_song_chart = lib::sqlite::prepare<SelectSongChart>(db);
	for (auto [
			song_path_sv, chart_path_sv, date_imported, title, subtitle, artist, subartist, genre,
			url, email, difficulty, playstyle, has_ln, has_soflan, note_count, chart_duration,
			audio_duration, loudness, average_nps, peak_nps, min_bpm, max_bpm, main_bpm,
			density_resolution, density_key, density_scratch, density_ln
		]: lib::sqlite::query(select_song_chart, md5)
	) {
		auto deserialize_density = [](span<byte const> v) {
			auto in = lib::bits::in(v);
			auto vec = vector<float>{};
			in(vec).or_throw();
			return vec;
		};
		song_path = fs::path{LibraryPath} / song_path_sv;
		chart_path = chart_path_sv;
		cache = Metadata{
			.title = string{title},
			.subtitle = string{subtitle},
			.artist = string{artist},
			.subartist = string{subartist},
			.genre = string{genre},
			.url = string{url},
			.email = string{email},
			.difficulty = static_cast<Difficulty>(difficulty),
			.playstyle = static_cast<Playstyle>(playstyle),
			.features = Metadata::Features{
				.has_ln = has_ln != 0,
				.has_soflan = has_soflan != 0,
			},
			.note_count = note_count,
			.chart_duration = nanoseconds{chart_duration},
			.audio_duration = nanoseconds{audio_duration},
			.loudness = loudness,
			.density = Metadata::Density{
				.resolution = nanoseconds{density_resolution},
				.key = deserialize_density(density_key),
				.scratch = deserialize_density(density_scratch),
				.ln = deserialize_density(density_ln),
			},
			.nps = Metadata::NPS{
				.average = static_cast<float>(average_nps),
				.peak = static_cast<float>(peak_nps),
			},
			.bpm_range = Metadata::BPMRange{
				.min = static_cast<float>(min_bpm),
				.max = static_cast<float>(max_bpm),
				.main = static_cast<float>(main_bpm),
			},
		};
		break;
	}
	if (!cache) throw runtime_error{"Chart not found"};

	auto song = io::Song(cat, io::read_file(song_path));
	auto chart_raw = song.load_file(chart_path);
	auto builder = Builder{cat};
	co_return co_await builder.build(pool, chart_raw, song, globals::mixer->get_audio().get_sampling_rate(), *cache);
}

auto Library::find_available_song_filename(string_view name) -> string
{
	for (auto i: views::iota(0u)) {
		auto test = i == 0?
			format("{}.zip", name) :
			format("{}-{}.zip", name, i);
		auto exists = false;
		auto song_exists = lib::sqlite::prepare<SongExists>(db);
		for (auto _: lib::sqlite::query(song_exists, test)) exists = true;
		if (!exists) return test;
	}
	unreachable();
}

auto Library::import_many(fs::path path) -> task<>
{
	if (fs::is_regular_file(path)) {
		import_stats.songs_total.fetch_add(1);
		co_await schedule_task_on(pool, import_one(path));
	} else if (fs::is_directory(path)) {
		auto contents = vector<fs::directory_entry>{};
		copy(fs::directory_iterator{path}, back_inserter(contents));
		if (any_of(contents, [&](auto const& entry) { return fs::is_regular_file(entry) && io::has_extension(entry, io::BMSExtensions); })) {
			import_stats.songs_total.fetch_add(1);
			co_await schedule_task_on(pool, import_one(path));
		} else {
			for (auto const& entry: contents) import_tasks.start(import_many(entry));
		}
	} else {
		ERROR_AS(cat, "Failed to import location \"{}\": unknown type of file", path);
		import_stats.songs_processed.fetch_add(1);
		import_stats.songs_failed.fetch_add(1);
	}
}

auto Library::import_one(fs::path path) -> task<>
{
	auto charts = vector<MD5>{}; // Need access to this in the catch clause
	try {
		if (stopping.load()) throw runtime_error_fmt("Song import \"{}\" cancelled", path);
		INFO_AS(cat, "Importing song \"{}\"", path);

		// Collect MD5s of charts to add
		auto source = io::Source{path};
		for (auto&& ref: source.for_each_file()) {
			if (!io::has_extension(ref.get_path(), io::BMSExtensions)) continue;
			charts.emplace_back(lib::openssl::md5(ref.read()));
		}

		// Check if any running task is a duplicate of this one
		auto lock = co_await staging_lock.scoped_lock();
		auto song_id = -1z;
		auto song_filename = string{};
		auto duplicate = false;
		for (auto const& chart: charts) {
			auto it = staging.find(chart);
			if (it != staging.end()) {
				song_id = it->second;
				duplicate = true;
				break;
			}
		}

		// Check if there is a duplicate song in the library
		if (!duplicate) {
			auto get_song_from_chart = lib::sqlite::prepare<GetSongFromChart>(db);
			for (auto const& chart: charts) {
				for (auto [id, _]: lib::sqlite::query(get_song_from_chart, chart)) song_id = id;
				if (song_id != -1z) {
					duplicate = true;
					break;
				}
			}
		}

		// Not duplicate? Obtain song ID
		if (!duplicate) {
			song_filename = source.is_archive()? path.stem().string() : path.filename().string();
			if (song_filename.empty())
				throw runtime_error_fmt("Failed to import \"{}\": invalid filename", path);
			song_filename = find_available_song_filename(song_filename);
			auto insert_song = lib::sqlite::prepare<InsertSong>(db);
			song_id = lib::sqlite::insert(insert_song, song_filename);
		}

		// Ensure exclusive ownership of song_id and associated songzip
		auto song_lock_it = song_locks.try_emplace(song_id).first;
		auto song_lock = co_await song_lock_it->second.scoped_lock();

		// Register intent to add charts
		for (auto const& chart: charts) staging.emplace(chart, song_id);
		lock.unlock();

		// Create/modify the songzip
		auto song = optional<io::Song>{nullopt};
		if (duplicate) {
			// Extending
			INFO_AS(cat, "Song \"{}\" already exists in library; extending", path);
			auto existing_song_path = fs::path{};
			auto select_song_by_id = lib::sqlite::prepare<SelectSongByID>(db);
			for (auto [pathname]: lib::sqlite::query(select_song_by_id, song_id))
				existing_song_path = fs::path{LibraryPath} / pathname;

			auto tmp_path = existing_song_path;
			tmp_path.concat(".tmp");
			auto deleter = io::FileDeleter{tmp_path};
			song = co_await io::Song::from_source_append(cat, pool, io::read_file(existing_song_path), source, tmp_path);

			fs::rename(tmp_path, existing_song_path);
			deleter.disarm();
		} else {
			// New song
			auto const out_path = fs::path{LibraryPath} / song_filename;
			auto deleter = io::FileDeleter{out_path};
			song = co_await io::Song::from_source(cat, pool, source, out_path);
			deleter.disarm();
		}

		// Prepare song for chart imports
		co_await song->preload_audio_files(pool, 48000);
		INFO_AS(cat, "Song \"{}\" files processed successfully", path);

		// Start chart imports
		auto chart_import_tasks = vector<task<MD5>>{};
		auto chart_paths = vector<string>{};
		for (auto [path, chart]: song->for_each_chart()) {
			chart_import_tasks.emplace_back(schedule_task_on(pool, import_chart(*song, song_id, string{path}, chart)));
			chart_paths.emplace_back(path);
		}
		auto results = co_await when_all(move(chart_import_tasks));

		// Report chart import results
		auto imported = vector<MD5>{};
		for (auto [result, path]: views::zip(results, chart_paths)) {
			try {
				auto md5 = result.return_value();
				if (md5 == MD5{}) continue; // Skipped
				INFO_AS(cat, "Chart \"{}\" imported successfully", path);
				imported.emplace_back(md5);
			} catch (exception const& e) {
				ERROR_AS(cat, "Failed to import chart \"{}\": {}", path, e.what());
				import_stats.charts_failed.fetch_add(1);
			}
		}

		// Clean up
		if (imported.empty()) {
			WARN_AS(cat, "No new charts found in song \"{}\"", path);
			if (!duplicate) {
				auto delete_song = lib::sqlite::prepare<DeleteSong>(db);
				lib::sqlite::execute(delete_song, song_id);
				move(*song).remove();
			}
		} else {
			auto deduplicated = co_await deduplicate_previews(song_id, imported);
			if (deduplicated)
				INFO_AS(cat, "Removed {} duplicate previews from song \"{}\"", deduplicated, path);
			INFO_AS(cat, "Song \"{}\" imported successfully", path);
		}
		import_stats.songs_processed.fetch_add(1);
	}
	catch (exception const& e) {
		ERROR_AS(cat, "Failed to import song \"{}\": {}", path, e.what());
		import_stats.songs_processed.fetch_add(1);
		import_stats.songs_failed.fetch_add(1);
	}
}

auto Library::import_chart(io::Song& song, ssize_t song_id, string chart_path, span<byte const> chart_raw) -> task<MD5>
{
	if (stopping.load()) throw runtime_error_fmt("Chart import \"{}\" cancelled", chart_path);

	auto chart_exists = lib::sqlite::prepare<ChartExists>(db);
	auto const md5 = lib::openssl::md5(chart_raw);
	auto exists = false;
	for (auto _: lib::sqlite::query(chart_exists, md5)) exists = true;
	if (exists) {
		INFO_AS(cat, "Chart import \"{}\" skipped (duplicate)", chart_path);
		import_stats.charts_skipped.fetch_add(1);
		co_return {};
	}

	auto insert_chart = lib::sqlite::prepare<InsertChart>(db);
	auto insert_chart_density = lib::sqlite::prepare<InsertChartDensity>(db);
	auto insert_chart_import_log = lib::sqlite::prepare<InsertChartImportLog>(db);
	auto insert_chart_preview = lib::sqlite::prepare<InsertChartPreview>(db);
	auto builder_cat = globals::logger->create_string_logger(lib::openssl::md5_to_hex(md5));
	INFO_AS(builder_cat, "Importing chart \"{}\"", chart_path);
	auto builder = Builder{builder_cat};
	auto chart = co_await builder.build(pool, chart_raw, song, 48000);
	auto encoded_preview = lib::ffmpeg::encode_as_opus(chart->media.preview, 48000);

	lib::sqlite::transaction(db, [&] {
		auto preview_id = lib::sqlite::insert(insert_chart_preview, encoded_preview);
		lib::sqlite::execute(insert_chart, chart->md5, song_id, chart_path, chart->metadata.title,
			chart->metadata.subtitle, chart->metadata.artist, chart->metadata.subartist,
			chart->metadata.genre, chart->metadata.url, chart->metadata.email,
			+chart->metadata.difficulty, +chart->metadata.playstyle, chart->metadata.features.has_ln,
			chart->metadata.features.has_soflan, chart->metadata.note_count,
			chart->metadata.chart_duration.count(), chart->metadata.audio_duration.count(),
			chart->metadata.loudness, chart->metadata.nps.average, chart->metadata.nps.peak,
			chart->metadata.bpm_range.min, chart->metadata.bpm_range.max,
			chart->metadata.bpm_range.main, preview_id);

		auto serialize_density = [](vector<float> const& v) {
			auto [data, out] = lib::bits::data_out();
			out(v).or_throw();
			return data;
		};
		lib::sqlite::execute(insert_chart_density, chart->md5,
			chart->metadata.density.resolution.count(),
			serialize_density(chart->metadata.density.key),
			serialize_density(chart->metadata.density.scratch),
			serialize_density(chart->metadata.density.ln));
		auto buffer = builder_cat.get_buffer();
		auto buffer_bytes = span{reinterpret_cast<byte const*>(buffer.data()), buffer.size() + 1};
		lib::sqlite::execute(insert_chart_import_log, chart->md5, lib::zstd::compress(buffer_bytes));
	});
	dirty.store(true);
	import_stats.charts_added.fetch_add(1);
	co_return chart->md5;
}

auto Library::deduplicate_previews(ssize_t song_id, span<MD5 const> new_charts) -> task<ssize_t> {
	// Some or all of the charts of this song were just added, all with their own previews.
	// Any of these previews can be a duplicate of a new preview or an old preview.

	// Fetch all previews (decoded) of all charts of the song, with their IDs.
	auto select_song_previews = lib::sqlite::prepare<SelectSongPreviews>(db);
	auto previews = unordered_map<ssize_t, vector<dev::Sample>>{};
	for (auto [id, preview]: lib::sqlite::query(select_song_previews, song_id))
		previews.emplace(id, lib::ffmpeg::decode_and_resample_file_buffer(preview, 48000));

	// Fetch all preview IDs of new charts
	auto select_chart_preview_ids = lib::sqlite::prepare<SelectChartPreviewIDs>(db);
	auto new_chart_preview_ids = vector<ssize_t>{};
	for (auto const& md5: new_charts) {
		for (auto [preview_id]: lib::sqlite::query(select_chart_preview_ids, md5))
			new_chart_preview_ids.emplace_back(preview_id);
	}

	// For every new preview, compute average sample difference from every other existing preview
	auto modify_chart_preview_ids = lib::sqlite::prepare<ModifyChartPreviewIDs>(db);
	auto delete_chart_preview = lib::sqlite::prepare<DeleteChartPreview>(db);
	auto previews_removed = 0z;
	for (auto [md5, preview_id]: views::zip(new_charts, new_chart_preview_ids)) {
		for (auto [id, preview]: previews) {
			if (preview_id == id) continue; // Don't check against yourself
			auto const& self = previews.at(preview_id);
			auto avg_diff = 0.0;
			auto const value_count = min(self.size(), preview.size());
			for (auto [left, right]: views::zip(self, preview)) {
				avg_diff += abs(left.left  - right.left)  / 2 / value_count;
				avg_diff += abs(left.right - right.right) / 2 / value_count;
			}
			if (avg_diff <= 0.0625) {
				// This is a duplicate
				previews.erase(preview_id);
				previews_removed += 1;
				lib::sqlite::transaction(db, [&] {
					lib::sqlite::execute(modify_chart_preview_ids, preview_id, id);
					lib::sqlite::execute(delete_chart_preview, preview_id);
				});
				break;
			}
		}
	}
	co_return previews_removed;
}

}

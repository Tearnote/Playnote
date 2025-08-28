/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

io/bulk_request.hpp:
A mechanism to request multiple files at once, to be fulfilled later on all at once.
*/

#pragma once
#include "preamble.hpp"
#include "logger.hpp"
#include "io/file.hpp"

namespace playnote::io {

class BulkRequest {
public:
	explicit BulkRequest(fs::path domain): domain{move(domain)} {}

	template<typename T>
	void enqueue(T::Output& output, string_view resource,
		initializer_list<string_view> extensions = {}, bool case_sensitive = true)
	{
		auto extensions_vec = vector<string>{};
		extensions_vec.reserve(extensions.size());
		transform(extensions, back_inserter(extensions_vec), [](auto& in) { return string{in}; });

		requests.emplace_back(Request{
			.resource = string{resource},
			.extensions = move(extensions_vec),
			.case_sensitive = case_sensitive,
			.processor = [&](span<byte const> raw) { output = move(T::process(raw)); },
		});
	}

	template<callable<void(usize, usize)> Func>
	void process(Func&& load_progress = [](usize, usize){})
	{
		INFO("Beginning file loads from \"{}\"", domain);
		load_progress(0, requests.size());

		// Enumerate available files
		auto file_list = vector<fs::path>{};
		for (auto const& entry: fs::recursive_directory_iterator{domain}) {
			if (!entry.is_regular_file()) continue;
			file_list.emplace_back(fs::relative(entry, domain));
		}

		// Process each request for matches in the file list
		auto matches = vector<fs::path>{};
		matches.reserve(file_list.size());
		struct LoadJob {
			usize request_idx;
			fs::path match;
		};
		auto jobs = vector<LoadJob>{};
		jobs.reserve(file_list.size());
		for (auto [idx, request]: views::enumerate(requests)) {
			auto match = find_if(file_list, [&](auto const& path) {
				auto path_str = path.string();

				// Handle case sensitivity (request is expected to already be lowercase)
				if (!request.case_sensitive)
					to_lower(path_str);

				// Handle extension matching
				if (!request.extensions.empty()) {
					for (auto const& ext: request.extensions) {
						if (path_str.ends_with(ext)) {
							path_str.resize(path_str.size() - ext.size());
							while (path_str.ends_with('.')) path_str.pop_back();
						}
					}
				}

				return path_str == request.resource;
			});
			if (match == file_list.end()) {
				WARN("Unable to load \"{}\": File not found", request.resource);
				continue;
			}
			jobs.emplace_back(LoadJob{
				.request_idx = idx,
				.match = domain / *match,
			});
			TRACE("Found \"{}\", reading at \"{}\"", request.resource, jobs.back().match);
		}

		auto completions = channel<usize>{};
		auto job_idx = atomic<usize>{0};

		auto worker = [&] {
			while (true) {
				auto const current_job_idx = job_idx.fetch_add(1);
				if (current_job_idx >= jobs.size()) break;

				auto const& job = jobs[current_job_idx];
				auto const& request = requests[job.request_idx];
				try {
					auto const raw = io::read_file(job.match);
					request.processor(raw.contents);
					completions << job.request_idx;
				} catch (exception const& e) {
					WARN("Failed to load \"{}\": {}", job.match, e.what());
					completions << -1zu;
				}
			}
		};

		auto const WorkerCount = jthread::hardware_concurrency();
		auto workers = vector<jthread>{};
		workers.reserve(WorkerCount);
		for (auto _: views::iota(0u, WorkerCount))
			workers.emplace_back(worker);

		auto completion_count = 0zu;
		auto const JobCount = jobs.size();
		auto completed = -1zu;
		while (completions.read(completed)) {
			completion_count += 1;
			load_progress(completion_count, JobCount);
			if (completion_count >= JobCount) break;
		}
		INFO("Finished reading files from \"{}\"", domain);
	}

private:
	struct Request {
		string resource;
		vector<string> extensions;
		bool case_sensitive;
		function<void(span<byte const>)> processor;
	};

	fs::path domain;
	vector<Request> requests;
};

}

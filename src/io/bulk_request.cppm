/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

io/bulk_request.cppm:
A mechanism to request multiple files at once, to be fulfilled later on all at once.
*/

module;
#include "preamble.hpp"
#include "logger.hpp"

export module playnote.io.bulk_request;

import playnote.io.file;

namespace playnote::io {

export class BulkRequest {
public:
	explicit BulkRequest(fs::path domain): domain{move(domain)} {}

	template<typename T>
	void enqueue(typename T::Output& output, string_view resource,
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

	template<callable<void(string_view, usize, usize)> Func>
	void process(Func&& before_load = [](string_view, usize, usize){})
	{
		INFO("Beginning file loads from \"{}\"", domain);

		// Enumerate available files
		vector<fs::path> file_list;
		for (auto const& entry: fs::recursive_directory_iterator{domain}) {
			if (!entry.is_regular_file()) continue;
			file_list.emplace_back(fs::relative(entry, domain));
		}

		// Process each request for matches in the file list
		for (auto [idx, request]: requests | views::enumerate) {
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
			TRACE("Found \"{}\", reading at \"{}\"", request.resource, domain / *match);
			before_load(request.resource, idx, requests.size());
			auto const raw = io::read_file(domain / *match);
			request.processor(raw.contents);
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

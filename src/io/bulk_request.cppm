/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

io/bulk_request.cppm:
A mechanism to request multiple files at once, to be fulfilled later on all at once.
*/

module;
#include <initializer_list>
#include <string_view>
#include <functional>
#include <algorithm>
#include <iterator>
#include <vector>
#include <string>
#include <span>

export module playnote.io.bulk_request;

import playnote.io.file;

namespace playnote::io {

export class BulkRequest {
public:
	explicit BulkRequest(std::string_view domain): domain(domain) {}

	template<typename T>
	void enqueue(typename T::Output& output, std::string_view resource,
		std::initializer_list<std::string_view> extensions = {})
	{
		auto extensions_vec = std::vector<std::string>{};
		std::ranges::transform(extensions, std::back_inserter(extensions_vec),
			[](auto const& ext) { return std::string{ext}; });
		requests.emplace_back(Request{
			.resource = std::string{resource},
			.extensions = std::move(extensions_vec),
			.processor = [&](std::span<char const> raw) {
				output = std::move(T::process(raw));
			},
		});
	}

	void process()
	{
		for (auto& request: requests) {
			auto raw = io::read_file(domain + "/" + request.resource); //todo extensions
			request.processor(raw.contents);
		}
	}

private:
	struct Request {
		std::string resource;
		std::vector<std::string> extensions;
		std::function<void(std::span<char const>)> processor;
	};

	std::string domain;
	std::vector<Request> requests;
};

}

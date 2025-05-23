/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

io/bulk_request.cppm:
A mechanism to request multiple files at once, to be fulfilled later on all at once.
*/

module;

export module playnote.io.bulk_request;

import playnote.preamble;
import playnote.io.file;

namespace playnote::io {

export class BulkRequest {
public:
	explicit BulkRequest(string domain): domain{move(domain)} {}

	template<typename T>
	void enqueue(typename T::Output& output, string resource, initializer_list<string> extensions = {})
	{
		requests.emplace_back(Request{
			.resource = move(resource),
			.extensions = move(extensions),
			.processor = [&](span<char const> raw) {
				output = move(T::process(raw));
			},
		});
	}

	void process()
	{
		for (auto& request: requests) {
			auto const path = format("{}/{}", domain, request.resource);
			auto const raw = io::read_file(path); //todo extensions
			request.processor(raw.contents);
		}
	}

private:
	struct Request {
		string resource;
		vector<string> extensions;
		function<void(span<char const>)> processor;
	};

	string domain;
	vector<Request> requests;
};

}

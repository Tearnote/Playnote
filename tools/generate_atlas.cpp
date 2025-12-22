/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "preamble.hpp"
#include "io/file.hpp"
#include "gfx/prewarm.hpp"
#include "gfx/text.hpp"
#include "utils/logger.hpp"

namespace playnote {

auto generate_atlas(span<char const* const> args)
try {
	if (args.size() < 3) {
		print(stderr, "Usage: {} <output> <fonts>...\n", args[0]);
		return EXIT_FAILURE;
	}
	auto* output_filename = args[1];
	auto font_filenames = args.subspan(2);

	auto logger_stub = globals::logger.provide("generate_atlas.log", Logger::Level::Debug);
	auto shaper = gfx::TextShaper{globals::logger->global};
	auto font_ids = vector<id>{};
	font_ids.reserve(font_filenames.size());
	for (auto font_path_sv: font_filenames) {
		auto font_path = fs::path{font_path_sv};
		auto font_filename = font_path.filename().string();
		auto font_id = id{font_filename};
		auto font_file = io::read_file(font_path);
		shaper.load_font(font_id, {font_file.contents.begin(), font_file.contents.end()}, 500);
		font_ids.emplace_back(font_id);
	}
	shaper.define_style("Sans-Medium"_id, font_ids);

	for (auto chars: gfx::AtlasPrewarmChars)
		shaper.shape("Sans-Medium"_id, chars);

	auto output = shaper.serialize();
	io::write_file(output_filename, output);
	return EXIT_SUCCESS;
}
catch (exception const& e) {
	print(stderr, "Uncaught exception: {}", e.what());
	return EXIT_FAILURE;
}

}

auto main(int argc, char** argv) -> int
{ return playnote::generate_atlas({argv, static_cast<std::size_t>(argc)}); }

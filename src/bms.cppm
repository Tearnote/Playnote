/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms.cppm:
BMS loading and parsing facilities.
*/

module;
#include "ankerl/unordered_dense.h"
#include "util/log_macros.hpp"

export module playnote.bms;

import playnote.stx.except;
import playnote.stx.types;
import playnote.util.charset;
import playnote.util.file;
import playnote.globals;

namespace playnote {

using stx::uint;
using util::UStringHash;
using util::UString;
using util::to_utf8;
template<typename Key, typename T, typename Hash>
using unordered_map = ankerl::unordered_dense::map<Key, T, Hash>;

//TODO Factor out file opening
//TODO Convert to BMSParser factory which produces BMS instances, to reuse header_handlers
export class BMS {
public:
	explicit BMS(std::string_view path);

private:
	struct HeaderParams {
		UString header;
		UString slot;
		UString value;
	};
	using HeaderHandlerFunc = void(BMS::*)(HeaderParams&&);
	unordered_map<UString, HeaderHandlerFunc, UStringHash> header_handlers{};

	void parse_header_ignored(HeaderParams&&) {}
	void parse_header_ignored_log(HeaderParams&& params) {L_INFO("Ignored header: {}", to_utf8(params.header)); }
	void parse_header_unimplemented(HeaderParams&& params) { L_WARN("Unimplemented header: {}", to_utf8(params.header)); }
	void parse_header_unimplemented_critical(HeaderParams&& params) { throw stx::runtime_error_fmt("Critical unimplemented header: {}", to_utf8(params.header)); }

	void parse_header_title(HeaderParams&& params) { title = std::move(params.value); }
	void parse_header_genre(HeaderParams&& params) { genre = std::move(params.value); }
	void parse_header_artist(HeaderParams&& params) { artist = std::move(params.value); }
	void parse_header_subartist(HeaderParams&& params) { subartist = std::move(params.value); }
	void register_header_handlers();

	void parse_file(UString&& file);
	void parse_line(UString&& line);
	void parse_channel(UString&& line);
	void parse_header(UString&& line);

	std::string path;
	UString title{};
	UString genre{};
	UString artist{};
	UString subartist{};
};

void BMS::register_header_handlers()
{
	// Implemented headers
	header_handlers.emplace("TITLE", &BMS::parse_header_title);
	header_handlers.emplace("ARTIST", &BMS::parse_header_artist);
	header_handlers.emplace("SUBARTIST", &BMS::parse_header_subartist);
	header_handlers.emplace("GENRE", &BMS::parse_header_genre);

	// Critical unimplemented headers
	// (if a file uses one of these, there is no chance for the BMS to play even remotely correctly)
	header_handlers.emplace("WAVCMD", &BMS::parse_header_unimplemented_critical);
	header_handlers.emplace("EXWAV", &BMS::parse_header_unimplemented_critical); // Underspecified, and likely unimplementable

	// Unimplemented headers
	header_handlers.emplace("PLAYER", &BMS::parse_header_unimplemented);
	header_handlers.emplace("VOLWAV", &BMS::parse_header_unimplemented);
	header_handlers.emplace("STAGEFILE", &BMS::parse_header_unimplemented);
	header_handlers.emplace("BANNER", &BMS::parse_header_unimplemented);
	header_handlers.emplace("BACKBMP", &BMS::parse_header_unimplemented);
	header_handlers.emplace("DIFFICULTY", &BMS::parse_header_unimplemented);
	header_handlers.emplace("SUBTITLE", &BMS::parse_header_unimplemented);
	header_handlers.emplace("MAKER", &BMS::parse_header_unimplemented);
	header_handlers.emplace("COMMENT", &BMS::parse_header_unimplemented);
	header_handlers.emplace("TEXT", &BMS::parse_header_unimplemented);
	header_handlers.emplace("SONG", &BMS::parse_header_unimplemented);
	header_handlers.emplace("BPM", &BMS::parse_header_unimplemented);
	header_handlers.emplace("EXBPM", &BMS::parse_header_unimplemented);
	header_handlers.emplace("BASEBPM", &BMS::parse_header_unimplemented);
	header_handlers.emplace("STOP", &BMS::parse_header_unimplemented);
	header_handlers.emplace("STP", &BMS::parse_header_unimplemented);
	header_handlers.emplace("LNTYPE", &BMS::parse_header_unimplemented);
	header_handlers.emplace("LNOBJ", &BMS::parse_header_unimplemented);
	header_handlers.emplace("OCT/FP", &BMS::parse_header_unimplemented);
	header_handlers.emplace("WAV", &BMS::parse_header_unimplemented);
	header_handlers.emplace("CDDA", &BMS::parse_header_unimplemented);
	header_handlers.emplace("MIDIFILE", &BMS::parse_header_unimplemented);
	header_handlers.emplace("BMP", &BMS::parse_header_unimplemented);
	header_handlers.emplace("BGA", &BMS::parse_header_unimplemented);
	header_handlers.emplace("@BGA", &BMS::parse_header_unimplemented);
	header_handlers.emplace("POORBGA", &BMS::parse_header_unimplemented);
	header_handlers.emplace("SWBGA", &BMS::parse_header_unimplemented);
	header_handlers.emplace("ARGB", &BMS::parse_header_unimplemented);
	header_handlers.emplace("VIDEOFILE", &BMS::parse_header_unimplemented);
	header_handlers.emplace("VIDEOf/s", &BMS::parse_header_unimplemented);
	header_handlers.emplace("VIDEOCOLORS", &BMS::parse_header_unimplemented);
	header_handlers.emplace("VIDEODLY", &BMS::parse_header_unimplemented);
	header_handlers.emplace("MOVIE", &BMS::parse_header_unimplemented);
	header_handlers.emplace("ExtChr", &BMS::parse_header_unimplemented);
	header_handlers.emplace("%URL", &BMS::parse_header_unimplemented);
	header_handlers.emplace("%EMAIL", &BMS::parse_header_unimplemented);

	// Unsupported headers
	header_handlers.emplace("RANK", &BMS::parse_header_ignored); // Playnote enforces uniform judgment
	header_handlers.emplace("DEFEXRANK", &BMS::parse_header_ignored); // ^
	header_handlers.emplace("EXRANK", &BMS::parse_header_ignored); // ^
	header_handlers.emplace("TOTAL", &BMS::parse_header_ignored); // Playnote enforces uniform gauges
	header_handlers.emplace("PLAYLEVEL", &BMS::parse_header_ignored); // Unreliable and useless
	header_handlers.emplace("DIVIDEPROP", &BMS::parse_header_ignored); // Not required
	header_handlers.emplace("CHARSET", &BMS::parse_header_ignored_log); // ^
	header_handlers.emplace("CHARFILE", &BMS::parse_header_ignored_log); // Unspecified
	header_handlers.emplace("SEEK", &BMS::parse_header_ignored_log); // ^
	header_handlers.emplace("EXBMP", &BMS::parse_header_ignored_log); // Underspecified (what's the blending mode?)
	header_handlers.emplace("PATH_WAV", &BMS::parse_header_ignored_log); // Security concern
	header_handlers.emplace("MATERIALS", &BMS::parse_header_ignored_log); // ^
	header_handlers.emplace("MATERIALSWAV", &BMS::parse_header_ignored_log); // ^
	header_handlers.emplace("MATERIALSBMP", &BMS::parse_header_ignored_log); // ^
	header_handlers.emplace("OPTION", &BMS::parse_header_ignored_log); // Horrifying, who invented this
	header_handlers.emplace("CHANGEOPTION", &BMS::parse_header_ignored_log); // ^
}

BMS::BMS(std::string_view path):
	path{path}
{
	L_INFO("Loading BMS file \"{}\"", path);
	auto file = util::read_file(path);
	auto encoding = util::detect_text_encoding(file.contents);
	if (encoding.empty()) {
		L_WARN("Failed to detect encoding, assuming Shift_JIS");
		encoding = "Shift_JIS";
	} else {
		L_TRACE("Detected encoding: {}", encoding);
	}
	auto contents = util::text_to_unicode(file.contents, encoding);
	L_TRACE("Converted \"{}\" to UTF-16", path);

	register_header_handlers();
	parse_file(std::move(contents));

	L_INFO("Loaded BMS file \"{}\" successfully", path);
	L_TRACE("Title: {}", to_utf8(title));
	L_TRACE("Genre: {}", to_utf8(genre));
	L_TRACE("Artist: {}", to_utf8(artist));
	L_TRACE("Subartist: {}", to_utf8(subartist));
}

void BMS::parse_file(UString&& file)
{
	// Normalize line endings
	file.findAndReplace("\r\n", "\n");
	file.findAndReplace("\r", "\n");

	auto pos = int{0};
	auto len = file.length();
	while (pos < len) {
		auto split_pos = file.indexOf('\n', pos);
		if (split_pos == -1)
			split_pos = len;
		parse_line(std::move(UString{file, pos, split_pos - pos}));
		pos = split_pos + 1; // Skip over the newline
	}
}

void BMS::parse_line(UString&& line)
{
	line.trim(); // BMS occasionally uses leading whitespace
	if (line.isEmpty()) return;
	if (line[0] != '#') return; // Ignore comments
	if (line[1] >= '0' && line[1] <= '9')
		parse_channel(std::move(line));
	else
		parse_header(std::move(line));
}

void BMS::parse_channel(UString&& line)
{
	// TODO
}

void BMS::parse_header(UString&& line)
{
	auto first_space = line.indexOf(' ');
	if (first_space == -1) first_space = line.length();
	auto first_tab = line.indexOf('\t');
	if (first_tab == -1) first_tab = line.length();
	auto first_whitespace = std::min(first_space, first_tab);

	auto header = UString{line, 1, first_whitespace - 1};
	header.toUpper();
	auto value = UString{line, first_whitespace + 1};

	auto slot = UString{};
	auto extract_slot = [&](int start) {
		slot = UString{header, start};
		header.truncate(start);
	};
	     if (header.startsWith("WAV"   )) extract_slot(3);
	else if (header.startsWith("BMP"   )) extract_slot(3);
	else if (header.startsWith("BGA"   )) extract_slot(3);
	else if (header.startsWith("BPM"   )) extract_slot(3); // This will also match BPM rather than BPMxx, but slot will end up empty anyway
	else if (header.startsWith("TEXT"  )) extract_slot(4);
	else if (header.startsWith("SONG"  )) extract_slot(4);
	else if (header.startsWith("@BGA"  )) extract_slot(4);
	else if (header.startsWith("STOP"  )) extract_slot(4);
	else if (header.startsWith("ARGB"  )) extract_slot(4);
	else if (header.startsWith("SEEK"  )) extract_slot(4);
	else if (header.startsWith("EXBPM" )) extract_slot(5);
	else if (header.startsWith("EXWAV" )) extract_slot(5);
	else if (header.startsWith("SWBGA")) extract_slot(5);
	else if (header.startsWith("EXRANK")) extract_slot(6);
	else if (header.startsWith("CHANGEOPTION")) extract_slot(12);

	try {
		(this->*header_handlers.at(header))({header, std::move(slot), std::move(value)});
	} catch (std::out_of_range const&) {
		auto header_u8 = std::string{};
		L_WARN("Unrecognized header in BMS file \"{}\": {}", path, header.toUTF8String(header_u8));
	}
}

}

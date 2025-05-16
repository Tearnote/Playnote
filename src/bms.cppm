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

export class BMS {
private:
	using HeaderHandlerFunc = void(BMS::*)(HeaderParams&&);
	unordered_map<UString, HeaderHandlerFunc, UStringHash> header_handlers{};

	void parse_header_ignored(HeaderParams&&) {}
	void parse_header_ignored_log(HeaderParams&& params) {L_INFO("Ignored header: {}", to_utf8(params.header)); }
	void parse_header_unimplemented(HeaderParams&& params) { L_WARN("Unimplemented header: {}", to_utf8(params.header)); }
	void parse_header_unimplemented_critical(HeaderParams&& params) { throw stx::runtime_error_fmt("Critical unimplemented header: {}", to_utf8(params.header)); }

	void parse_header_title(HeaderParams&& params) { title = std::move(params.value); }
	void parse_header_subtitle(HeaderParams&& params) { subtitle = std::move(params.value); }
	void parse_header_genre(HeaderParams&& params) { genre = std::move(params.value); }
	void parse_header_artist(HeaderParams&& params) { artist = std::move(params.value); }
	void parse_header_subartist(HeaderParams&& params) { subartist = std::move(params.value); }
	void parse_header_url(HeaderParams&& params) { url = std::move(params.value); }
	void parse_header_email(HeaderParams&& params) { email = std::move(params.value); }
	void register_header_handlers();
};

void BMS::register_header_handlers()
{
	// Implemented headers
	header_handlers.emplace("TITLE", &BMS::parse_header_title);
	header_handlers.emplace("SUBTITLE", &BMS::parse_header_subtitle);
	header_handlers.emplace("ARTIST", &BMS::parse_header_artist);
	header_handlers.emplace("SUBARTIST", &BMS::parse_header_subartist);
	header_handlers.emplace("GENRE", &BMS::parse_header_genre);
	header_handlers.emplace("%URL", &BMS::parse_header_url);
	header_handlers.emplace("%EMAIL", &BMS::parse_header_email);

	// Critical unimplemented headers
	// (if a file uses one of these, there is no chance for the BMS to play even remotely correctly)
	header_handlers.emplace("WAVCMD", &BMS::parse_header_unimplemented_critical);
	header_handlers.emplace("EXWAV", &BMS::parse_header_unimplemented_critical); // Underspecified, and likely unimplementable
	header_handlers.emplace("RANDOM", &BMS::parse_header_unimplemented_critical);
	header_handlers.emplace("IF", &BMS::parse_header_unimplemented_critical);
	header_handlers.emplace("ELSEIF", &BMS::parse_header_unimplemented_critical);
	header_handlers.emplace("ELSE", &BMS::parse_header_unimplemented_critical);
	header_handlers.emplace("ENDIF", &BMS::parse_header_unimplemented_critical);
	header_handlers.emplace("SETRANDOM", &BMS::parse_header_unimplemented_critical);
	header_handlers.emplace("ENDRANDOM", &BMS::parse_header_unimplemented_critical);
	header_handlers.emplace("SWITCH", &BMS::parse_header_unimplemented_critical);
	header_handlers.emplace("CASE", &BMS::parse_header_unimplemented_critical);
	header_handlers.emplace("SKIP", &BMS::parse_header_unimplemented_critical);
	header_handlers.emplace("DEF", &BMS::parse_header_unimplemented_critical);
	header_handlers.emplace("SETSWITCH", &BMS::parse_header_unimplemented_critical);
	header_handlers.emplace("ENDSW", &BMS::parse_header_unimplemented_critical);

	// Unimplemented headers
	header_handlers.emplace("PLAYER", &BMS::parse_header_unimplemented);
	header_handlers.emplace("VOLWAV", &BMS::parse_header_unimplemented);
	header_handlers.emplace("STAGEFILE", &BMS::parse_header_unimplemented);
	header_handlers.emplace("BANNER", &BMS::parse_header_unimplemented);
	header_handlers.emplace("BACKBMP", &BMS::parse_header_unimplemented);
	header_handlers.emplace("DIFFICULTY", &BMS::parse_header_unimplemented);
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

}

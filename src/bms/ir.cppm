/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/ir.cppm:
The Intermediate Representation of a BMS file. Equivalent in functionality to the original file,
but condensed, cleaned and serializable to a binary file. Handles IR compilation from BMS file
commands.
*/

module;
#include <memory_resource>
#include <string_view>
#include <utility>
#include <variant>
#include <memory>
#include <string>
#include <openssl/evp.h>
#include "ankerl/unordered_dense.h"
#include "util/log_macros.hpp"

export module playnote.bms.ir;

import playnote.stx.concepts;
import playnote.stx.except;
import playnote.stx.types;
import playnote.util.charset;
import playnote.bms.parser;
import playnote.globals;

namespace playnote::bms {

template<typename Key, typename T, typename Hash>
using unordered_map = ankerl::unordered_dense::map<Key, T, Hash>;
using stx::uint8;
using util::UStringHash;
using util::UString;
using util::to_utf8;
using bms::HeaderCommand;

export class IRCompiler;

export class IR {
public:
	struct HeaderEvent {
		struct Title {
			UString title;
		};
		struct Subtitle {
			UString subtitle;
		};
		struct Artist {
			UString artist;
		};
		using ParamsType = std::variant<
			std::monostate, // 0
			Title*,         // 1
			Subtitle*,      // 2
			Artist*         // 3
		>;

		ParamsType params;
	};

private:
	friend IRCompiler;

	std::unique_ptr<std::pmr::monotonic_buffer_resource> buffer_resource; // unique_ptr makes it moveable
	std::pmr::polymorphic_allocator<> allocator;

	std::string path;
	std::array<uint8, 16> md5{};
	std::pmr::vector<HeaderEvent> header_events;

	IR();

	template<typename T>
		requires stx::is_variant_alternative<T*, HeaderEvent::ParamsType>
	void add_header_event(T&&);
};

template<typename T>
	requires stx::is_variant_alternative<T*, IR::HeaderEvent::ParamsType>
void IR::add_header_event(T&& event)
{
	header_events.emplace_back(HeaderEvent{
		.params = allocator.new_object<T>(std::forward<T>(event)),
	});
}

export class IRCompiler {
public:
	IRCompiler();

	auto compile(std::string_view path, std::string_view bms_file_contents) -> IR;

private:
	using HeaderHandlerFunc = void(IRCompiler::*)(IR&, HeaderCommand&&);
	unordered_map<UString, HeaderHandlerFunc, UStringHash> header_handlers{};

	void register_header_handlers();

	void parse_header_ignored(IR&, HeaderCommand&&) {}
	void parse_header_ignored_log(IR&, HeaderCommand&&);
	void parse_header_unimplemented(IR&, HeaderCommand&&);
	void parse_header_unimplemented_critical(IR&, HeaderCommand&&);

	void parse_header_title(IR&, HeaderCommand&&);
	void parse_header_subtitle(IR&, HeaderCommand&&);
	void parse_header_artist(IR&, HeaderCommand&&);
};

IR::IR():
	buffer_resource{std::make_unique<std::pmr::monotonic_buffer_resource>()},
	allocator{buffer_resource.get()},
	header_events{allocator} {}

IRCompiler::IRCompiler()
{
	register_header_handlers();
}

auto IRCompiler::compile(std::string_view path, std::string_view bms_file_contents) -> IR
{
	L_INFO("Compiling BMS file \"{}\"", path);
	auto ir = IR{};

	ir.path = std::string{path};
	EVP_Q_digest(nullptr, "MD5", nullptr, bms_file_contents.data(), bms_file_contents.size(), ir.md5.data(), nullptr);

	bms::parse(bms_file_contents,
		[&](HeaderCommand&& cmd) { (this->*header_handlers.at(cmd.header))(ir, std::move(cmd)); },
		[](ChannelCommand&& cmd) {
			// L_TRACE("{}: #{}{}:{}", cmd.line, cmd.measure, to_utf8(cmd.channel), to_utf8(cmd.value));
		}
	);

	return ir;
}

void IRCompiler::register_header_handlers()
{
	// Implemented headers
	header_handlers.emplace("TITLE", &IRCompiler::parse_header_title);
	header_handlers.emplace("SUBTITLE", &IRCompiler::parse_header_subtitle);
	header_handlers.emplace("ARTIST", &IRCompiler::parse_header_artist);

	// Critical unimplemented headers
	// (if a file uses one of these, there is no chance for the BMS to play even remotely correctly)
	header_handlers.emplace("WAVCMD", &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("EXWAV", &IRCompiler::parse_header_unimplemented_critical); // Underspecified, and likely unimplementable
	header_handlers.emplace("RANDOM", &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("IF", &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("ELSEIF", &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("ELSE", &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("ENDIF", &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("SETRANDOM", &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("ENDRANDOM", &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("SWITCH", &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("CASE", &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("SKIP", &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("DEF", &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("SETSWITCH", &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("ENDSW", &IRCompiler::parse_header_unimplemented_critical);

	// Unimplemented headers
	header_handlers.emplace("SUBARTIST", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("GENRE", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("%URL", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("%EMAIL", &IRCompiler::parse_header_unimplemented);

	header_handlers.emplace("PLAYER", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("VOLWAV", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("STAGEFILE", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("BANNER", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("BACKBMP", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("DIFFICULTY", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("MAKER", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("COMMENT", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("TEXT", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("SONG", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("BPM", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("EXBPM", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("BASEBPM", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("STOP", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("STP", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("LNTYPE", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("LNOBJ", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("OCT/FP", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("WAV", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("CDDA", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("MIDIFILE", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("BMP", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("BGA", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("@BGA", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("POORBGA", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("SWBGA", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("ARGB", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("VIDEOFILE", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("VIDEOf/s", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("VIDEOCOLORS", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("VIDEODLY", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("MOVIE", &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("ExtChr", &IRCompiler::parse_header_unimplemented);

	// Unsupported headers
	header_handlers.emplace("RANK", &IRCompiler::parse_header_ignored); // Playnote enforces uniform judgment
	header_handlers.emplace("DEFEXRANK", &IRCompiler::parse_header_ignored); // ^
	header_handlers.emplace("EXRANK", &IRCompiler::parse_header_ignored); // ^
	header_handlers.emplace("TOTAL", &IRCompiler::parse_header_ignored); // Playnote enforces uniform gauges
	header_handlers.emplace("PLAYLEVEL", &IRCompiler::parse_header_ignored); // Unreliable and useless
	header_handlers.emplace("DIVIDEPROP", &IRCompiler::parse_header_ignored); // Not required
	header_handlers.emplace("CHARSET", &IRCompiler::parse_header_ignored_log); // ^
	header_handlers.emplace("CHARFILE", &IRCompiler::parse_header_ignored_log); // Unspecified
	header_handlers.emplace("SEEK", &IRCompiler::parse_header_ignored_log); // ^
	header_handlers.emplace("EXBMP", &IRCompiler::parse_header_ignored_log); // Underspecified (what's the blending mode?)
	header_handlers.emplace("PATH_WAV", &IRCompiler::parse_header_ignored_log); // Security concern
	header_handlers.emplace("MATERIALS", &IRCompiler::parse_header_ignored_log); // ^
	header_handlers.emplace("MATERIALSWAV", &IRCompiler::parse_header_ignored_log); // ^
	header_handlers.emplace("MATERIALSBMP", &IRCompiler::parse_header_ignored_log); // ^
	header_handlers.emplace("OPTION", &IRCompiler::parse_header_ignored_log); // Horrifying, who invented this
	header_handlers.emplace("CHANGEOPTION", &IRCompiler::parse_header_ignored_log); // ^
}

void IRCompiler::parse_header_ignored_log(IR&, HeaderCommand&& cmd)
{
	L_INFO("Ignored header: {}", to_utf8(cmd.header));
}

void IRCompiler::parse_header_unimplemented(IR&, HeaderCommand&& cmd)
{
	L_WARN("Unimplemented header: {}", to_utf8(cmd.header));
}

void IRCompiler::parse_header_unimplemented_critical(IR&, HeaderCommand&& cmd)
{
	throw stx::runtime_error_fmt("Critical unimplemented header: {}", to_utf8(cmd.header));
}

void IRCompiler::parse_header_title(IR& ir, HeaderCommand&& cmd)
{
	if (cmd.value.isEmpty()) {
		L_WARN("Title header has no value");
		return;
	}

	L_TRACE("Title: {}", to_utf8(cmd.value));
	ir.add_header_event(IR::HeaderEvent::Title{
		.title = std::move(cmd.value),
	});
}

void IRCompiler::parse_header_subtitle(IR& ir, HeaderCommand&& cmd)
{
	if (cmd.value.isEmpty()) {
		L_WARN("Subtitle header has no value");
		return;
	}

	L_TRACE("Subtitle: {}", to_utf8(cmd.value));
	ir.add_header_event(IR::HeaderEvent::Subtitle{
		.subtitle = std::move(cmd.value),
	});
}

void IRCompiler::parse_header_artist(IR& ir, HeaderCommand&& cmd)
{
	if (cmd.value.isEmpty()) {
		L_WARN("Artist header has no value");
		return;
	}

	L_TRACE("Artist: {}", to_utf8(cmd.value));
	ir.add_header_event(IR::HeaderEvent::Artist{
		.artist = std::move(cmd.value),
	});
}

}

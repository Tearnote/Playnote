/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

util/logger.cppm:
Wrapper for the Quill threaded async logging library.
*/

module;
#include "libassert/assert.hpp"
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/FileSink.h"
#include "quill/Frontend.h"
#include "quill/Backend.h"
#include "quill/Logger.h"

export module playnote.util.logger;

import playnote.preamble;

namespace playnote::util {

using namespace quill;

export inline auto log_category_global = static_cast<Frontend::logger_t*>(nullptr);

export class Logger {
public:
	Logger(string const& log_file_path, LogLevel);
	auto create_category(string const& name, LogLevel = LogLevel::TraceL1,
		bool log_to_console = true, bool log_to_file = true) -> Frontend::logger_t&;

private:
	PatternFormatterOptions const Pattern{
		"%(time) [%(log_level_short_code)] [%(logger)] %(message)",
		"%H:%M:%S.%Qms"
	};
	static auto constexpr ShortCodes = to_array<string>({
		"TR3", "TR2", "TRA", "DBG", "INF", "NTC",
		"WRN", "ERR", "CRT", "BCT", "___", "DYN"
	});

	shared_ptr<ConsoleSink> console_sink;
	shared_ptr<FileSink> file_sink;
};

Logger::Logger(string const& log_file_path, LogLevel global_log_level)
{
	ASSUME(!log_category_global); // Avoid double-initialization
	Backend::start<FrontendOptions>({
		.thread_name = "Logging",
		.enable_yield_when_idle = true,
		.sleep_duration = 0ns,
		.log_level_short_codes = ShortCodes,
	}, SignalHandlerOptions{});

	console_sink = static_pointer_cast<ConsoleSink>(
		Frontend::create_or_get_sink<ConsoleSink>("console"));

	auto file_cfg = FileSinkConfig{};
	file_cfg.set_open_mode('w');
	file_sink = static_pointer_cast<FileSink>(
		Frontend::create_or_get_sink<FileSink>(log_file_path, file_cfg));

	log_category_global = &create_category("Global", global_log_level);
}

auto Logger::create_category(string const& name, LogLevel level, bool log_to_console,
	bool log_to_file) -> Frontend::logger_t&
{
	auto* category = Frontend::create_or_get_logger(name, {
		log_to_console? console_sink : nullptr,
		log_to_file? file_sink : nullptr
	}, Pattern);
	category->set_log_level(level);
	return *category;
}

}

/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "utils/logger.hpp"

#include "quill/Backend.h"
#include "preamble.hpp"

namespace playnote {

static auto const StringLoggerPattern = quill::PatternFormatterOptions{
	"%(time) [%(log_level_short_code)] %(message)",
	"%H:%M:%S.%Qms"
};

static auto const LoggerPattern = quill::PatternFormatterOptions{
	"%(time) [%(log_level_short_code)] [%(logger)] %(message)",
	"%H:%M:%S.%Qms"
};

static auto const ShortCodes = to_array<string>({
	"TR3", "TR2", "TRA", "DBG", "INF", "NTC",
	"WRN", "ERR", "CRT", "BCT", "___"
});

Logger::StringLogger::StringLogger(string_view name, Logger::Level level)
{
	auto name_str = string{name};
	sink = static_pointer_cast<MemorySink>(quill::Frontend::create_or_get_sink<MemorySink>(name_str));
	category = quill::Frontend::create_or_get_logger(name_str, {sink}, StringLoggerPattern);
	category->set_log_level(level);
}

Logger::StringLogger::~StringLogger()
{
	quill::Frontend::remove_logger(category);
}

auto Logger::StringLogger::get_buffer() -> string
{
	category->flush_log();
	return sink->get_buffer();
}

auto Logger::StringLogger::MemorySink::get_buffer() -> string
{
	auto out_buffer = move(buffer);
	buffer = string{};
	return out_buffer;
}

Logger::Logger(string_view log_file_path, Level global_log_level)
{
	quill::Backend::start<quill::FrontendOptions>({
		.thread_name = "Logging",
		.enable_yield_when_idle = true,
		.sleep_duration = 0ns,
		.error_notifier = [&](string const& msg) { WARN_AS(global, "{}", msg); },
		.check_printable_char = {}, // Allow UTF-8
		.log_level_short_codes = ShortCodes,
	}, quill::SignalHandlerOptions{});

	console_sink = static_pointer_cast<quill::ConsoleSink>(
		quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console"));

	auto file_cfg = quill::FileSinkConfig{};
	file_cfg.set_open_mode('w');
	file_sink = static_pointer_cast<quill::FileSink>(
		quill::Frontend::create_or_get_sink<quill::FileSink>(string{log_file_path}, file_cfg));

	global = create_category("Global", global_log_level);
}

auto Logger::create_category(string_view name, Level level, bool log_to_console, bool log_to_file) -> Category
{
	auto* category = quill::Frontend::create_or_get_logger(string{name}, {
		log_to_console? console_sink : nullptr,
		log_to_file? file_sink : nullptr
	}, LoggerPattern);
	category->set_log_level(level);
	return category;
}

auto Logger::create_string_logger(string_view name, Level level) -> StringLogger
{
	return StringLogger{name, level};
}

}

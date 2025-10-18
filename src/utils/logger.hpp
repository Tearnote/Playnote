/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

util/logger.hpp:
Wrapper for the Quill threaded async logging library.
*/

#pragma once
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/FileSink.h"
#include "quill/LogMacros.h"
#include "quill/Frontend.h"
#include "quill/Backend.h"
#include "quill/Logger.h"
#include "preamble.hpp"
#include "utils/service.hpp"

// Workaround for compiler warning on Windows
#ifdef ERROR
#undef ERROR
#endif

#define TRACE(...) LOG_TRACE_L1(globals::logger->global, __VA_ARGS__)
#define DEBUG(...) LOG_DEBUG(globals::logger->global, __VA_ARGS__)
#define INFO(...) LOG_INFO(globals::logger->global, __VA_ARGS__)
#define WARN(...) LOG_WARNING(globals::logger->global, __VA_ARGS__)
#define ERROR(...) LOG_ERROR(globals::logger->global, __VA_ARGS__)
#define CRIT(...) LOG_CRITICAL(globals::logger->global, __VA_ARGS__)

// Log to a specific category

#define TRACE_AS(category, ...) LOG_TRACE_L1(category, __VA_ARGS__)
#define DEBUG_AS(category, ...) LOG_DEBUG(category, __VA_ARGS__)
#define INFO_AS(category, ...) LOG_INFO(category, __VA_ARGS__)
#define WARN_AS(category, ...) LOG_WARNING(category, __VA_ARGS__)
#define ERROR_AS(category, ...) LOG_ERROR(category, __VA_ARGS__)
#define CRIT_AS(category, ...) LOG_CRITICAL(category, __VA_ARGS__)

namespace playnote {

class Logger {
public:
	using Category = quill::Frontend::logger_t;
	using Level = quill::LogLevel;

	Category* global = nullptr;

	// Initialize the logger. A global category will be created, immediately usable
	// with the global logging macros.
	Logger(string_view log_file_path, Level);

	// Create a new category. To be used with the *_AS macros.
	auto create_category(string_view name, Level = Level::TraceL1,
		bool log_to_console = true, bool log_to_file = true) -> Category*;

private:
	static inline auto const Pattern = quill::PatternFormatterOptions{
		"%(time) [%(log_level_short_code)] [%(logger)] %(message)",
		"%H:%M:%S.%Qms"
	};
	static inline auto const ShortCodes = to_array<string>({
		"TR3", "TR2", "TRA", "DBG", "INF", "NTC",
		"WRN", "ERR", "CRT", "BCT", "___"
	});

	shared_ptr<quill::ConsoleSink> console_sink;
	shared_ptr<quill::FileSink> file_sink;
};

inline Logger::Logger(string_view log_file_path, Level global_log_level)
{
	quill::Backend::start<quill::FrontendOptions>({
		.thread_name = "Logging",
		.enable_yield_when_idle = true,
		.sleep_duration = 0ns,
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

inline auto Logger::create_category(string_view name, Level level, bool log_to_console,
	bool log_to_file) -> Category*
{
	auto* category = quill::Frontend::create_or_get_logger(string{name}, {
		log_to_console? console_sink : nullptr,
		log_to_file? file_sink : nullptr
	}, Pattern);
	category->set_log_level(level);
	return category;
}

}

namespace playnote::globals {
inline auto logger = Service<Logger>{};
}

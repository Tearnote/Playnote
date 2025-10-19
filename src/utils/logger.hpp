/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

util/logger.hpp:
Wrapper for the Quill threaded async logging library.
*/

#pragma once
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/StreamSink.h"
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

#define TRACE_AS(category, ...) LOG_TRACE_L1(static_cast<quill::Frontend::logger_t*>(category), __VA_ARGS__)
#define DEBUG_AS(category, ...) LOG_DEBUG(static_cast<quill::Frontend::logger_t*>(category), __VA_ARGS__)
#define INFO_AS(category, ...) LOG_INFO(static_cast<quill::Frontend::logger_t*>(category), __VA_ARGS__)
#define WARN_AS(category, ...) LOG_WARNING(static_cast<quill::Frontend::logger_t*>(category), __VA_ARGS__)
#define ERROR_AS(category, ...) LOG_ERROR(static_cast<quill::Frontend::logger_t*>(category), __VA_ARGS__)
#define CRIT_AS(category, ...) LOG_CRITICAL(static_cast<quill::Frontend::logger_t*>(category), __VA_ARGS__)

namespace playnote {

// Point of access to the logging system.
class Logger {
public:
	// A named tag for log messages. Its destinations and level can be customized independently.
	using Category = quill::Frontend::logger_t*;
	// Log importance level.
	using Level = quill::LogLevel;

	// A special category that writes all log messages into an owned string buffer.
	// Use create_string_logger() to get an instance.
	class StringLogger {
	public:
		~StringLogger();

		// Retrieve the string with all log messages so far.
		// The existing buffer is moved out, and a new one is created in its place.
		auto get_buffer() -> string;

		// Allow usage in *_AS macros.
		operator Logger::Category() { return category; }

	private:
		class MemorySink: public quill::Sink {
		public:
			MemorySink() = default;

			void write_log(quill::MacroMetadata const*, uint64_t /** log_timestamp **/,
				std::string_view /** thread_id **/, std::string_view /** thread_name **/,
				std::string const& /** process_id **/, std::string_view /** logger_name **/,
				quill::LogLevel, std::string_view /** log_level_description **/,
				std::string_view /** log_level_short_code **/,
				std::vector<std::pair<std::string, std::string>> const* /** named_args **/,
				std::string_view /** log_message **/, std::string_view log_statement) override
			{
				buffer.append(log_statement);
			}

			auto get_buffer() -> string;

			void flush_sink() noexcept override {}
			void run_periodic_tasks() noexcept override {}

			MemorySink(MemorySink const&) = delete;
			auto operator=(MemorySink const&) -> MemorySink& = delete;
			MemorySink(MemorySink&&) = delete;
			auto operator=(MemorySink&&) -> MemorySink& = delete;

		private:
			string buffer;
		};

		friend class Logger;

		static inline auto const Pattern = quill::PatternFormatterOptions{
			"%(time) [%(log_level_short_code)] %(message)",
			"%H:%M:%S.%Qms"
		};

		string buffer;
		shared_ptr<MemorySink> sink;
		Logger::Category category;

		StringLogger(string_view name, Logger::Level);
	};

	// Public access to the global logging category.
	Category global = nullptr;

	// Initialize the logger. A global category will be created, immediately usable
	// with the global logging macros.
	Logger(string_view log_file_path, Level);

	// Create a new category. To be used with the *_AS macros.
	// If the category already exists, the previously created instance is returned.
	auto create_category(string_view name, Level = Level::TraceL1,
		bool log_to_console = true, bool log_to_file = true) -> Category;

	// Create a new category that logs into a string buffer.
	auto create_string_logger(string_view name, Level = Level::TraceL1) -> StringLogger;

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

inline Logger::StringLogger::StringLogger(string_view name, Logger::Level level)
{
	auto name_str = string{name};
	sink = static_pointer_cast<MemorySink>(quill::Frontend::create_or_get_sink<MemorySink>(name_str));
	category = quill::Frontend::create_or_get_logger(name_str, {sink}, Pattern);
	category->set_log_level(level);
}

inline Logger::StringLogger::~StringLogger()
{
	quill::Frontend::remove_logger(category);
}

inline auto Logger::StringLogger::get_buffer() -> string
{
	category->flush_log();
	return sink->get_buffer();
}

inline auto Logger::StringLogger::MemorySink::get_buffer() -> string
{
	auto out_buffer = move(buffer);
	buffer = string{};
	return out_buffer;
}

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

inline auto Logger::create_category(string_view name, Level level, bool log_to_console, bool log_to_file) -> Category
{
	auto* category = quill::Frontend::create_or_get_logger(string{name}, {
		log_to_console? console_sink : nullptr,
		log_to_file? file_sink : nullptr
	}, Pattern);
	category->set_log_level(level);
	return category;
}

inline auto Logger::create_string_logger(string_view name, Level level) -> StringLogger
{
	return StringLogger{name, level};
}

}

namespace playnote::globals {
inline auto logger = Service<Logger>{};
}

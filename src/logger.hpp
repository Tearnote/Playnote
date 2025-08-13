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
#include "assert.hpp"

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
	class Impl {
	public:
		using Category = quill::Frontend::logger_t;
		using Level = quill::LogLevel;

		Category* global{nullptr};

		Impl(string_view log_file_path, Level);
		auto register_category(string_view name, Level = Level::TraceL1,
			bool log_to_console = true, bool log_to_file = true) -> Category*;
		auto get_category(string_view name) -> Category* { return categories.at(name); }

	private:
		static inline auto const Pattern = quill::PatternFormatterOptions{
			"%(time) [%(log_level_short_code)] [%(logger)] %(message)",
			"%H:%M:%S.%Qms"
		};
		static inline auto const ShortCodes = to_array<string>({
			"TR3", "TR2", "TRA", "DBG", "INF", "NTC",
			"WRN", "ERR", "CRT", "BCT", "___", "DYN"
		});

		shared_ptr<quill::ConsoleSink> console_sink;
		shared_ptr<quill::FileSink> file_sink;
		unordered_map<string_view, Category*> categories;
	};

	class Stub {
	public:
		Stub(Logger& parent, string_view log_file_path, Impl::Level global_level):
			logger{log_file_path, global_level}
		{
			parent.handle = &logger;
		}
		Stub(Stub const&) = delete;
		auto operator=(Stub const&) -> Stub& = delete;
		Stub(Stub&&) = delete;
		auto operator=(Stub&&) -> Stub& = delete;

	private:
		Impl logger;
	};

public:
	using Category = Impl::Category;
	using Level = Impl::Level;

	auto provide(string_view log_file_path, Impl::Level global_level) -> Stub
	{
		return Stub{*this, log_file_path, global_level};
	}

	auto operator*() -> Impl& { return *ASSUME_VAL(handle); }
	auto operator->() -> Impl* { return ASSUME_VAL(handle); }
	operator bool() const { return handle != nullptr; } // NOLINT(*-explicit-constructor)

private:
	Impl* handle{nullptr};
};

inline Logger::Impl::Impl(string_view log_file_path, Level global_log_level)
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

	global = register_category("Global", global_log_level);
}

inline auto Logger::Impl::register_category(string_view name, Level level, bool log_to_console,
	bool log_to_file) -> Category*
{
	auto* category = quill::Frontend::create_or_get_logger(string{name}, {
		log_to_console? console_sink : nullptr,
		log_to_file? file_sink : nullptr
	}, Pattern);
	category->set_log_level(level);
	categories.emplace(name, category);
	return category;
}

}

namespace playnote::globals {
inline auto logger = Logger{};
}

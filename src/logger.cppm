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

export module playnote.logger;

import playnote.preamble;

namespace playnote {

using namespace quill;

export class Logger {
	class Impl {
	public:
		using Category = Frontend::logger_t;
		using Level = LogLevel;

		Category* global{nullptr};

		Impl(string_view log_file_path, LogLevel);
		auto register_category(string_view name, Level = Level::TraceL1,
			bool log_to_console = true, bool log_to_file = true) -> Category*;
		auto get_category(string_view name) -> Category* { return categories.at(name); }

	private:
		static inline auto const Pattern = PatternFormatterOptions{
			"%(time) [%(log_level_short_code)] [%(logger)] %(message)",
			"%H:%M:%S.%Qms"
		};
		static auto constexpr ShortCodes = to_array<string>({
			"TR3", "TR2", "TRA", "DBG", "INF", "NTC",
			"WRN", "ERR", "CRT", "BCT", "___", "DYN"
		});

		shared_ptr<ConsoleSink> console_sink;
		shared_ptr<FileSink> file_sink;
		unordered_map<string_view, Category*> categories;
	};

	class Stub {
	public:
		Stub(Logger& parent, string_view log_file_path, LogLevel global_level):
			parent{parent}, logger{log_file_path, global_level}
		{
			parent.handle = &logger;
		}
		Stub(Stub const&) = delete;
		auto operator=(Stub const&) -> Stub& = delete;
		Stub(Stub&&) = delete;
		auto operator=(Stub&&) -> Stub& = delete;

	private:
		Logger& parent;
		Impl logger;
	};

public:
	using Category = Impl::Category;
	using Level = Impl::Level;

	auto provide(string_view log_file_path, LogLevel global_level) -> Stub
	{
		return Stub{*this, log_file_path, global_level};
	}

	auto operator*() -> Impl& { return *ASSUME_VAL(handle); }
	auto operator->() -> Impl* { return ASSUME_VAL(handle); }
	operator bool() const { return handle != nullptr; } // NOLINT(*-explicit-constructor)

private:
	Impl* handle{nullptr};
};

Logger::Impl::Impl(string_view log_file_path, LogLevel global_log_level)
{
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
		Frontend::create_or_get_sink<FileSink>(string{log_file_path}, file_cfg));

	global = register_category("Global", global_log_level);
}

auto Logger::Impl::register_category(string_view name, Level level, bool log_to_console,
	bool log_to_file) -> Category*
{
	auto* category = Frontend::create_or_get_logger(string{name}, {
		log_to_console? console_sink : nullptr,
		log_to_file? file_sink : nullptr
	}, Pattern);
	category->set_log_level(level);
	categories.emplace(name, category);
	return category;
}

}

namespace playnote::globals {
export inline auto logger = Logger{};
}

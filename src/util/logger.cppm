module;
#include <utility>
#include "libassert/assert.hpp"
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/FileSink.h"
#include "quill/Frontend.h"
#include "quill/Backend.h"
#include "quill/Logger.h"
#include "config.hpp"

export module playnote.util.logger;

namespace playnote::util {

// Wrapper for the Quill threaded async logging library
export class Logger {
public:
	Logger();
	[[nodiscard]] auto get() -> quill::Logger* { return logger; }

private:
	inline static quill::Logger* logger{nullptr}; // Static to ensure only one can exist
};

Logger::Logger()
{
	ASSUME(!logger);
	quill::Backend::start({
		.thread_name = "Logging",
		.log_level_short_codes = {
			"TR3", "TR2", "TRA", "DBG", "INF", "NTC",
			"WRN", "ERR", "CRT", "BCT", "___", "DYN"
		},
	});
	auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console");
	auto file_cfg = quill::FileSinkConfig{};
	file_cfg.set_open_mode('w');
	auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(LogfilePath, file_cfg);
	logger = quill::Frontend::create_or_get_logger("root",
		{std::move(console_sink), std::move(file_sink)},
		quill::PatternFormatterOptions{
			"%(time) [%(log_level_short_code)] %(message)",
			"%H:%M:%S.%Qms"
		}
	);
	logger->set_log_level(LoggingLevel);
}

}

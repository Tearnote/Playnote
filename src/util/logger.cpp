#include "util/logger.hpp"

#include <utility>
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/FileSink.h"
#include "quill/Frontend.h"
#include "quill/Backend.h"
#include "config.hpp"

Logger_impl::Logger_impl() {
	auto& self = *this;

	quill::Backend::start({
		.thread_name = "Logging",
		.log_level_short_codes = {
			"TR3", "TR2", "TRA", "DBG", "INF", "NTC",
			"WRN", "ERR", "CRT", "BCT", "___", "DYN"
		},
	});
	auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console");
	auto file_cfg = quill::FileSinkConfig();
	file_cfg.set_open_mode('w');
	auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(LogfilePath, file_cfg);
	self.logger = quill::Frontend::create_or_get_logger("root", {std::move(console_sink), std::move(file_sink)},
		quill::PatternFormatterOptions("%(time) [%(log_level_short_code)] %(message)", "%H:%M:%S.%Qms"));
	self.logger->set_log_level(LoggingLevel);
}

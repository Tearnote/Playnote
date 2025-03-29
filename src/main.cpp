#include <utility>
#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/Logger.h"
#include "quill/LogMacros.h"
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/FileSink.h"
#include "config.hpp"

auto main(int argc, char* argv[]) -> int {
	quill::Backend::start();
	auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console");
	auto file_cfg = quill::FileSinkConfig();
	file_cfg.set_open_mode('w');
	auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(LogfilePath, file_cfg, quill::FileEventNotifier());
	auto* logger = quill::Frontend::create_or_get_logger("root", {std::move(console_sink), std::move(file_sink)});
	logger->set_log_level(LoggingLevel);
	LOG_INFO(logger, "Hello World!");
}

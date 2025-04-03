#pragma once

#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "util/service.hpp"

class Logger
{
public:
	// Only access via macros below
	quill::Logger* logger;

	Logger();
};

inline auto s_logger = Service<Logger>();

// Convenience macros

#define L_TRACE(...) LOG_TRACE_L1(s_logger->logger, __VA_ARGS__)
#define L_DEBUG(...) LOG_DEBUG(s_logger->logger, __VA_ARGS__)
#define L_INFO(...) LOG_INFO(s_logger->logger, __VA_ARGS__)
#define L_WARN(...) LOG_WARNING(s_logger->logger, __VA_ARGS__)
#define L_ERROR(...) LOG_ERROR(s_logger->logger, __VA_ARGS__)
#define L_CRIT(...) LOG_CRITICAL(s_logger->logger, __VA_ARGS__)

#pragma once

#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "util/service.hpp"

class Logger_impl {
public:
	// Only access via macros below
	quill::Logger* logger;

	Logger_impl();
};

using Logger = Service<Logger_impl>;

// Convenience macros for logger service users

#define L_TRACE(...) LOG_TRACE_L1(Logger::serv->logger, __VA_ARGS__)
#define L_DEBUG(...) LOG_DEBUG(Logger::serv->logger, __VA_ARGS__)
#define L_INFO(...) LOG_INFO(Logger::serv->logger, __VA_ARGS__)
#define L_WARN(...) LOG_WARNING(Logger::serv->logger, __VA_ARGS__)
#define L_ERROR(...) LOG_ERROR(Logger::serv->logger, __VA_ARGS__)
#define L_CRIT(...) LOG_CRITICAL(Logger::serv->logger, __VA_ARGS__)

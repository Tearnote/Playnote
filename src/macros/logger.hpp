/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

macros/logger.hpp:
Macros for easy access to the global logger.
Requires playnote.logger to be imported, and for the logger inside to be instantiated.
*/

#ifndef PLAYNOTE_MACROS_LOGGER_HPP
#define PLAYNOTE_MACROS_LOGGER_HPP

#include "quill/LogMacros.h"

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

#endif

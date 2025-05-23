/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

util/log_macros.hpp:
Macros for easy access to the global logger.
Requires playnote.util.logger to be imported, and for log_category_global inside to be instantiated.
*/

#ifndef UTIL_LOG_MACROS_HPP
#define UTIL_LOG_MACROS_HPP

#include "quill/LogMacros.h"

#define L_TRACE(...) LOG_TRACE_L1(util::log_category_global, __VA_ARGS__)
#define L_DEBUG(...) LOG_DEBUG(util::log_category_global, __VA_ARGS__)
#define L_INFO(...) LOG_INFO(util::log_category_global, __VA_ARGS__)
#define L_WARN(...) LOG_WARNING(util::log_category_global, __VA_ARGS__)
#define L_ERROR(...) LOG_ERROR(util::log_category_global, __VA_ARGS__)
#define L_CRIT(...) LOG_CRITICAL(util::log_category_global, __VA_ARGS__)

#endif

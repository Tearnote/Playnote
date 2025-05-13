#ifndef UTIL_LOG_MACROS_HPP
#define UTIL_LOG_MACROS_HPP

#include "quill/LogMacros.h"

// Logging macros
// Requires importing playnote.globals, and for g_logger to be instantiated
#define L_TRACE(...) LOG_TRACE_L1(playnote::g_logger->get(), __VA_ARGS__)
#define L_DEBUG(...) LOG_DEBUG(playnote::g_logger->get(), __VA_ARGS__)
#define L_INFO(...) LOG_INFO(playnote::g_logger->get(), __VA_ARGS__)
#define L_WARN(...) LOG_WARNING(playnote::g_logger->get(), __VA_ARGS__)
#define L_ERROR(...) LOG_ERROR(playnote::g_logger->get(), __VA_ARGS__)
#define L_CRIT(...) LOG_CRITICAL(playnote::g_logger->get(), __VA_ARGS__)

#endif

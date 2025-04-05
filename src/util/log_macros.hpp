#ifndef UTIL_LOG_MACROS_HPP
#define UTIL_LOG_MACROS_HPP

import playnote.util.logger;
#include "quill/LogMacros.h"

#define L_TRACE(...) LOG_TRACE_L1(playnote::util::s_logger->get(), __VA_ARGS__)
#define L_DEBUG(...) LOG_DEBUG(playnote::util::s_logger->get(), __VA_ARGS__)
#define L_INFO(...) LOG_INFO(playnote::util::s_logger->get(), __VA_ARGS__)
#define L_WARN(...) LOG_WARNING(playnote::util::s_logger->get(), __VA_ARGS__)
#define L_ERROR(...) LOG_ERROR(playnote::util::s_logger->get(), __VA_ARGS__)
#define L_CRIT(...) LOG_CRITICAL(playnote::util::s_logger->get(), __VA_ARGS__)

#endif

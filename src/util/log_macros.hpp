#ifndef UTIL_LOG_MACROS_HPP
#define UTIL_LOG_MACROS_HPP

#include "quill/LogMacros.h"
import playnote.util.logger;
import playnote.globals;

#define L_TRACE(...) LOG_TRACE_L1(playnote::locator.get<playnote::util::Logger>().get(), __VA_ARGS__)
#define L_DEBUG(...) LOG_DEBUG(playnote::locator.get<playnote::util::Logger>().get(), __VA_ARGS__)
#define L_INFO(...) LOG_INFO(playnote::locator.get<playnote::util::Logger>().get(), __VA_ARGS__)
#define L_WARN(...) LOG_WARNING(playnote::locator.get<playnote::util::Logger>().get(), __VA_ARGS__)
#define L_ERROR(...) LOG_ERROR(playnote::locator.get<playnote::util::Logger>().get(), __VA_ARGS__)
#define L_CRIT(...) LOG_CRITICAL(playnote::locator.get<playnote::util::Logger>().get(), __VA_ARGS__)

#endif

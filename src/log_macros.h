
#ifndef LOG_MACROS_H
#define LOG_MACROS_H

#if defined(LOG_DEBUG)
    #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#endif

#if defined(LOG_TRACE)
    #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include "spdlog/spdlog.h"

class LoggerHelper {
    public:
    static void setupLoggers() {
        #if defined(LOG_DEBUG)
            spdlog::set_level(spdlog::level::debug);
        #endif

        #if defined(LOG_TRACE)
            spdlog::set_level(spdlog::level::trace);
        #endif
    }
};

#endif
#ifndef LOG_H
#define LOG_H

#include <llprint.h>

enum log_level {
    FATAL     = 0,
    ERROR     = 1,
    WARNING   = 2,
    INFO      = 3,
    DEBUGGING = 4,
    TRACE     = 5
};


#ifndef LOGGING_LEVEL
#define LOGGING_LEVEL WARNING
#endif

#define LOG_FATAL(...) do { if(LOGGING_LEVEL >= FATAL) printc(__VA_ARGS__) } while(0)

#define LOG_ERROR(...) do { if(LOGGING_LEVEL >= ERROR) printc(__VA_ARGS__) } while(0)

#define LOG_WARNING(...) do { if(LOGGING_LEVEL >= WARNING) printc(__VA_ARGS__) } while(0)

#define LOG_INFO(...) do { if(LOGGING_LEVEL >= INFO) printc(__VA_ARGS__) } while(0)

#define LOG_DEBUG(...) do { if(LOGGING_LEVEL >= DEBUG) printc(__VA_ARGS__) } while(0)

#define LOG_TRACE(...) do { if(LOGGING_LEVEL >= TRACE) printc(__VA_ARGS__) } while(0)


#endif

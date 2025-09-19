#pragma once
#include <coreinit/debug.h>
#include <cstring>

#define __FILENAME__ ({                                \
    const char *__filename = __FILE__;                 \
    const char *__pos      = strrchr(__filename, '/'); \
    if (!__pos) __pos = strrchr(__filename, '\\');     \
    __pos ? __pos + 1 : __filename;                    \
})

#define LOG_APP_TYPE "L"
#define LOG_APP_NAME "libmocha"

#define LOG_EX(FILENAME, FUNCTION, LINE, LOG_FUNC, LOG_LEVEL, LINE_END, FMT, ARGS...)                                                        \
    do {                                                                                                                                     \
        LOG_FUNC("[(%s)%18s][%23s]%30s@L%04d: " LOG_LEVEL "" FMT "" LINE_END, LOG_APP_TYPE, LOG_APP_NAME, FILENAME, FUNCTION, LINE, ##ARGS); \
    } while (0)

#define LOG_EX_DEFAULT(LOG_FUNC, LOG_LEVEL, LINE_END, FMT, ARGS...) LOG_EX(__FILENAME__, __FUNCTION__, __LINE__, LOG_FUNC, LOG_LEVEL, LINE_END, FMT, ##ARGS)

#define DEBUG_FUNCTION_LINE_ERR(FMT, ARGS...)                       LOG_EX_DEFAULT(OSReport, "##ERROR## ", "\n", FMT, ##ARGS)
#define DEBUG_FUNCTION_LINE_WARN(FMT, ARGS...)                      LOG_EX_DEFAULT(OSReport, "##WARNING## ", "\n", FMT, ##ARGS)

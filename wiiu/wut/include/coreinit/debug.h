#pragma once
#include <wut.h>

#ifdef __cplusplus
extern "C" {
#endif

void OSConsoleWrite(const char *msg, uint32_t size);
void OSReport(const char *fmt, ...);
void OSPanic(const char *file, uint32_t line, const char *fmt, ...);
void OSFatal(const char *msg);

#ifdef __cplusplus
}
#endif

#pragma once
#include <stddef.h>
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void OSConsoleWrite(const char *msg, int size);
void OSReport(const char *fmt, ...);
void OSPanic(const char *file, int line, const char *fmt, ...);
void OSFatal(const char *msg);

int __os_snprintf(char *buf, int n, const char *format, ... );

#ifdef __cplusplus
}
#endif

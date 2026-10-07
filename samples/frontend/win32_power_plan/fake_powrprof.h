/* The powrprof, kernel32 and logging surface the lifted power plan
 * code uses, standing in for <windows.h>; the test supplies the
 * functions. */

#ifndef FAKE_POWRPROF_H
#define FAKE_POWRPROF_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <boolean.h>

#define WINAPI

typedef unsigned long DWORD;
typedef unsigned char UCHAR;
typedef wchar_t       WCHAR;
typedef void         *HKEY;
typedef void         *HMODULE;
/* GCC lets void (*)(void) cast to any function type without
 * -Wcast-function-type, as it must for GetProcAddress results. */
typedef void (*FARPROC)(void);

/* Laid out without padding, so memcmp over it compares the value */
typedef struct
{
   uint32_t       Data1;
   unsigned short Data2;
   unsigned short Data3;
   unsigned char  Data4[8];
} GUID;

#define ERROR_SUCCESS 0UL
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

HMODULE LoadLibraryA(const char *name);
FARPROC GetProcAddress(HMODULE module, const char *name);
void   *LocalFree(void *mem);
void    fake_log(const char *fmt, ...);
#define RARCH_WARN fake_log

#endif

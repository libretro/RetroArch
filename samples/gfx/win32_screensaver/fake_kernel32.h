/* The kernel32 surface win32_suspend_screensaver() uses, standing in
 * for <windows.h>; the test supplies the functions. */

#ifndef FAKE_KERNEL32_H
#define FAKE_KERNEL32_H

#include <stddef.h>
#include <boolean.h>

#define WINAPI

typedef unsigned long ULONG;
typedef unsigned long DWORD;
typedef int           BOOL;
typedef void         *HANDLE;
typedef void         *HMODULE;
typedef wchar_t      *LPWSTR;
/* GCC lets void (*)(void) cast to any function type without
 * -Wcast-function-type, as it must for GetProcAddress results. */
typedef void (*FARPROC)(void);

#define INVALID_HANDLE_VALUE ((HANDLE)(ptrdiff_t)-1)

HMODULE GetModuleHandleA(const char *name);
FARPROC GetProcAddress(HMODULE module, const char *name);

bool win32_suspend_screensaver(void *data, bool enable);

#endif

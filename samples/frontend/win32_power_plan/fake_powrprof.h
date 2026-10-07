/* The powrprof, kernel32, registry and logging surface the lifted
 * power plan code uses, standing in for <windows.h>; the test supplies
 * the functions. */

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
typedef unsigned char BYTE;
typedef unsigned char *LPBYTE;
typedef long          LONG;
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

#define HKEY_CURRENT_USER ((HKEY)(uintptr_t)0x80000001)
#define KEY_READ  0x20019
#define KEY_WRITE 0x20006
#define REG_DWORD 4

HMODULE LoadLibraryA(const char *name);
FARPROC GetProcAddress(HMODULE module, const char *name);
void   *LocalFree(void *mem);
LONG    RegOpenKeyExA(HKEY root, const char *sub, DWORD opt, DWORD sam,
      HKEY *out);
LONG    RegCreateKeyExA(HKEY root, const char *sub, DWORD reserved,
      char *cls, DWORD opt, DWORD sam, void *sa, HKEY *out, DWORD *disp);
LONG    RegQueryValueExA(HKEY key, const char *name, DWORD *reserved,
      DWORD *type, LPBYTE data, DWORD *size);
LONG    RegSetValueExA(HKEY key, const char *name, DWORD reserved,
      DWORD type, const BYTE *data, DWORD size);
LONG    RegDeleteValueA(HKEY key, const char *name);
LONG    RegCloseKey(HKEY key);
void    fake_log(const char *fmt, ...);
#define RARCH_WARN fake_log

#endif

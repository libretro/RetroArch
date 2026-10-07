/* The shell32, user32, kernel32 and dylib surface the lifted drag and
 * drop code uses, standing in for <windows.h>; the test supplies the
 * functions. */

#ifndef FAKE_WIN32_H
#define FAKE_WIN32_H

#include <stddef.h>
#include <stdlib.h>
#include <wchar.h>
#include <boolean.h>
#include <lists/string_list.h>

#define WINAPI
#define VOID void
#define TRUE  1
#define FALSE 0

typedef unsigned int  UINT;
typedef unsigned long DWORD;
typedef long          HRESULT;
typedef int           BOOL;
typedef void         *HWND;
typedef void         *HMODULE;
typedef void         *HDROP;
typedef size_t        WPARAM;
typedef wchar_t      *LPWSTR;
typedef void         *dylib_t;
/* GCC lets void (*)(void) cast to any function type without
 * -Wcast-function-type, as it must for GetProcAddress results. */
typedef void (*FARPROC)(void);
typedef void (*function_t)(void);

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

HMODULE GetModuleHandleA(const char *name);
FARPROC GetProcAddress(HMODULE module, const char *name);
UINT DragQueryFileA(HDROP drop, UINT index, char *buf, UINT len);

dylib_t    dylib_load(const char *path);
function_t dylib_proc(dylib_t lib, const char *proc);
void       dylib_close(dylib_t lib);

int  fake_atexit(void (*fn)(void));
void fake_log(const char *fmt, ...);
#define atexit     fake_atexit
#define RARCH_WARN fake_log

bool local_to_utf8_string(const char *in, char *s, size_t len);
char *utf16_to_utf8_string_alloc(const wchar_t *str);
bool menu_driver_drop(struct string_list *files);
bool win32_load_content_from_gui(const char *path);

extern VOID (WINAPI *DragAcceptFiles_func)(HWND, BOOL);

#endif

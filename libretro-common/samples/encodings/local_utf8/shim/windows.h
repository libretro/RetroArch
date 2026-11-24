/* The two Win32 conversions encoding_utf.c calls, so its Windows path
 * runs on Linux. The local codepage is made up: 0x81-0x9F lead a
 * two-byte character (U+3000 up, three bytes of UTF-8), 0xA0-0xFF are
 * Latin-1 (two bytes of UTF-8). A buffer too short gets 0 and is
 * scribbled on, as Windows leaves it undefined.
 */
#ifndef SHIM_WINDOWS_H
#define SHIM_WINDOWS_H

#include <stddef.h>
#include <string.h>
#include <wchar.h>

#define CP_ACP  0
#define CP_UTF8 65001

typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef int BOOL;
typedef char CHAR;
typedef wchar_t WCHAR;
typedef const char *LPCSTR;
typedef char *LPSTR;
typedef const wchar_t *LPCWSTR;
typedef wchar_t *LPWSTR;
typedef BOOL *LPBOOL;

extern unsigned shim_mbtowc_calls;
extern unsigned shim_wctomb_calls;

int MultiByteToWideChar(UINT cp, DWORD flags, LPCSTR in, int in_len,
      LPWSTR out, int out_len);
int WideCharToMultiByte(UINT cp, DWORD flags, LPCWSTR in, int in_len,
      LPSTR out, int out_len, LPCSTR def, LPBOOL used_def);

#endif

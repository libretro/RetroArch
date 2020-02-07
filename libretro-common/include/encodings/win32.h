/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (utf.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _LIBRETRO_ENCODINGS_WIN32_H
#define _LIBRETRO_ENCODINGS_WIN32_H

#ifndef _XBOX
#ifdef _WIN32
/*#define UNICODE
#include <tchar.h>
#include <wchar.h>*/

#ifdef __cplusplus
extern "C" {
#endif

#include <encodings/utf.h>

#ifdef __cplusplus
}
#endif

#endif
#endif

#ifdef UNICODE
#define CHAR_TO_WCHAR_ALLOC(s, ws) \
   size_t ws##_size = (NULL != s && s[0] ? strlen(s) : 0) + 1; \
   wchar_t *ws = (wchar_t*)calloc(ws##_size, 2); \
   if (NULL != s && s[0]) \
      MultiByteToWideChar(CP_UTF8, 0, s, -1, ws, ws##_size / sizeof(wchar_t));

#define WCHAR_TO_CHAR_ALLOC(ws, s) \
   size_t s##_size = ((NULL != ws && ws[0] ? wcslen((const wchar_t*)ws) : 0) / 2) + 1; \
   char *s = (char*)calloc(s##_size, 1); \
   if (NULL != ws && ws[0]) \
      utf16_to_char_string((const uint16_t*)ws, s, s##_size);

#else
#define CHAR_TO_WCHAR_ALLOC(s, ws) char *ws = (NULL != s && s[0] ? strdup(s) : NULL);
#define WCHAR_TO_CHAR_ALLOC(ws, s) char *s = (NULL != ws && ws[0] ? strdup(ws) : NULL);
#endif

#endif

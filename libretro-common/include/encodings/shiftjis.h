/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (shiftjis.h).
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

#ifndef _LIBRETRO_ENCODINGS_SHIFTJIS_H
#define _LIBRETRO_ENCODINGS_SHIFTJIS_H

#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Shift-JIS here is Windows code page 932: JIS X 0208 with the NEC and
 * IBM extensions, without the user-defined area. Each conversion skips
 * input it cannot convert, an octet at a time, writes at most
 * @out_size - 1 octets and a NUL, stopping before a character that does
 * not fit, and returns the octets written, the NUL not counted. */

/* An @out_size of 3 * @in_len + 1 always suffices. */
size_t sjis_to_utf8(char *out, size_t out_size, const char *in, size_t in_len);

/* An @out_size of @in_len + 1 always suffices. */
size_t utf8_to_sjis(char *out, size_t out_size, const char *in, size_t in_len);

/* Windows code page 1252; an @out_size of 3 * @in_len + 1 always
 * suffices. */
size_t cp1252_to_utf8(char *out, size_t out_size, const char *in, size_t in_len);

RETRO_END_DECLS

#endif

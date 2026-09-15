/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rstrtod.h).
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

/* Decimal to binary floating point, correctly rounded.
 *
 * strtod() from the C library would do this, except that it reads the
 * decimal separator from the current locale: the same settings file
 * parses differently once someone's LC_NUMERIC says a comma. These
 * functions always take '.', never consult the locale, and never touch
 * errno, so a value written on one machine reads back the same on
 * another.
 *
 * The result is the nearest double (or float) to the exact decimal
 * value, ties resolved to even -- the same answer a correctly rounded
 * strtod() gives, for every input. Where the C library differs, the C
 * library is wrong.
 *
 * The caller passes a length rather than relying on a terminator, so a
 * number sitting inside a larger buffer needs no copy. On success the
 * end pointer reports how much was consumed, which lets a caller reject
 * trailing junk it did not expect.
 *
 * Accepted: an optional sign, decimal digits with an optional '.', an
 * optional 'e'/'E' exponent with optional sign, and the special forms
 * "inf", "infinity" and "nan" in any case. Values too large become
 * infinity and values too small become zero, both with the right sign;
 * neither is reported as an error, matching strtod().
 */

#ifndef __LIBRETRO_SDK_STRING_RSTRTOD_H
#define __LIBRETRO_SDK_STRING_RSTRTOD_H

#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/**
 * rstrtod_len:
 * @str                    : bytes to read, need not be NUL-terminated
 * @len                    : length of @str
 * @end                    : filled with how many bytes were consumed
 *
 * Reads a decimal number, ignoring the locale. Leading whitespace is
 * skipped. @end may be NULL; when it is not, it receives 0 if nothing
 * could be read, in which case the return value is 0.0.
 *
 * Returns: the nearest double to the value read, ties to even.
 */
double rstrtod_len(const char *str, size_t len, size_t *end);

/**
 * rstrtof_len:
 * @str                    : bytes to read, need not be NUL-terminated
 * @len                    : length of @str
 * @end                    : filled with how many bytes were consumed
 *
 * As rstrtod_len(), rounding once to float rather than twice through
 * double -- a double rounding would be wrong for some inputs.
 *
 * Returns: the nearest float to the value read, ties to even.
 */
float rstrtof_len(const char *str, size_t len, size_t *end);

/**
 * rstrtod:
 * @str                    : NUL-terminated bytes to read
 * @end                    : filled with a pointer past what was consumed
 *
 * The strtod()-shaped spelling, for callers that have a C string.
 * @end may be NULL.
 *
 * Returns: the nearest double to the value read.
 */
double rstrtod(const char *str, char **end);

/**
 * rstrtof:
 * @str                    : NUL-terminated bytes to read
 * @end                    : filled with a pointer past what was consumed
 *
 * Returns: the nearest float to the value read.
 */
float rstrtof(const char *str, char **end);

RETRO_END_DECLS

#endif

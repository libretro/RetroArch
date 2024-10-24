/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rjson.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RJSON_HELPERS_H__
#define __LIBRETRO_SDK_FORMAT_RJSON_HELPERS_H__

#include <retro_common_api.h>
#include <retro_inline.h> /* INLINE */
#include <boolean.h> /* bool */
#include <stddef.h> /* size_t */

RETRO_BEGIN_DECLS

/* Functions to add JSON token characters */
static INLINE void rjsonwriter_add_start_object(rjsonwriter_t *writer)
      { rjsonwriter_raw(writer, "{", 1); }

static INLINE void rjsonwriter_add_end_object(rjsonwriter_t *writer)
      { rjsonwriter_raw(writer, "}", 1); }

static INLINE void rjsonwriter_add_start_array(rjsonwriter_t *writer)
      { rjsonwriter_raw(writer, "[", 1); }

static INLINE void rjsonwriter_add_end_array(rjsonwriter_t *writer)
      { rjsonwriter_raw(writer, "]", 1); }

static INLINE void rjsonwriter_add_colon(rjsonwriter_t *writer)
      { rjsonwriter_raw(writer, ":", 1); }

static INLINE void rjsonwriter_add_comma(rjsonwriter_t *writer)
      { rjsonwriter_raw(writer, ",", 1); }

/* Functions to add whitespace characters */
/* These do nothing with the option RJSONWRITER_OPTION_SKIP_WHITESPACE */
static INLINE void rjsonwriter_add_newline(rjsonwriter_t *writer)
      { rjsonwriter_raw(writer, "\n", 1); }

static INLINE void rjsonwriter_add_space(rjsonwriter_t *writer)
      { rjsonwriter_raw(writer, " ", 1); }

static INLINE void rjsonwriter_add_tab(rjsonwriter_t *writer)
      { rjsonwriter_raw(writer, "\t", 1); }

static INLINE void rjsonwriter_add_unsigned(rjsonwriter_t *writer, unsigned value)
      { rjsonwriter_rawf(writer, "%u", value); }

/* Add a signed or unsigned integer or a double number */
static INLINE void rjsonwriter_add_int(rjsonwriter_t *writer, int value)
      { rjsonwriter_rawf(writer, "%d", value); }

static INLINE void rjsonwriter_add_bool(rjsonwriter_t *writer, bool value)
      { rjsonwriter_raw(writer, (value ? "true" : "false"), (value ? 4 : 5)); }

static INLINE void rjsonwriter_add_null(rjsonwriter_t *writer)
      { rjsonwriter_raw(writer, "null", 4); }

RETRO_END_DECLS

#endif

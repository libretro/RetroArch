/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (stdstring.h).
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

#ifndef __LIBRETRO_SDK_STDSTRING_H
#define __LIBRETRO_SDK_STDSTRING_H

#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include <boolean.h>

#include <retro_common_api.h>
#include <retro_inline.h>

RETRO_BEGIN_DECLS

static INLINE bool string_is_empty(const char *data)
{
   return (data == NULL) || (*data == '\0');
}

static INLINE bool string_is_equal(const char *a, const char *b)
{
   if (!a || !b)
      return false;
   while(*a && (*a == *b))
      a++, b++;
   return (*(const unsigned char*)a - *(const unsigned char*)b) == 0;
}

static INLINE bool string_is_equal_noncase(const char *a, const char *b)
{
   bool ret;
   int i;
   char *cp1, *cp2;

   if (!a || !b)
      return false;

   cp1 = (char*)malloc(strlen(a) + 1);
   cp2 = (char*)malloc(strlen(b) + 1);

   for (i = 0; i < strlen(a) + 1; i++)
      cp1[i] = tolower((int) (unsigned char) a[i]);
   for (i = 0; i < strlen(b) + 1; i++)
      cp2[i] = tolower((int) (unsigned char) b[i]);

   ret = string_is_equal(cp1, cp2);

   free(cp1);
   free(cp2);

   return ret;
}

char *string_to_upper(char *s);

char *string_to_lower(char *s);

char *string_ucwords(char* s);

char *string_replace_substring(const char *in, const char *pattern,
      const char *by);

/* Remove leading whitespaces */
char *string_trim_whitespace_left(char *const s);

/* Remove trailing whitespaces */
char *string_trim_whitespace_right(char *const s);

/* Remove leading and trailing whitespaces */
char *string_trim_whitespace(char *const s);

char *word_wrap(char* buffer, const char *string, int line_width);

RETRO_END_DECLS

#endif

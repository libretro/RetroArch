/* Copyright  (C) 2010-2019 The RetroArch team
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
#include <compat/strl.h>

RETRO_BEGIN_DECLS

static INLINE bool string_is_empty(const char *data)
{
   return !data || (*data == '\0');
}

static INLINE bool string_is_equal(const char *a, const char *b)
{
   return (a && b) ? !strcmp(a, b) : false;
}

#define STRLEN_CONST(x)                   ((sizeof((x))-1))

#define string_is_not_equal(a, b)         !string_is_equal((a), (b))

#define string_is_not_equal_fast(a, b, size) (memcmp(a, b, size) != 0)
#define string_is_equal_fast(a, b, size)     (memcmp(a, b, size) == 0)

static INLINE bool string_is_equal_case_insensitive(const char *a,
      const char *b)
{
   int result              = 0;
   const unsigned char *p1 = (const unsigned char*)a;
   const unsigned char *p2 = (const unsigned char*)b;

   if (!a || !b)
      return false;
   if (p1 == p2)
      return true;

   while ((result = tolower (*p1) - tolower (*p2++)) == 0)
      if (*p1++ == '\0')
         break;

   return (result == 0);
}

static INLINE bool string_is_equal_noncase(const char *a, const char *b)
{
   int result              = 0;
   const unsigned char *p1 = (const unsigned char*)a;
   const unsigned char *p2 = (const unsigned char*)b;

   if (!a || !b)
      return false;
   if (p1 == p2)
      return false;

   while ((result = tolower (*p1) - tolower (*p2++)) == 0)
      if (*p1++ == '\0')
         break;

   return (result == 0);
}

char *string_to_upper(char *s);

char *string_to_lower(char *s);

char *string_ucwords(char *s);

char *string_replace_substring(const char *in, const char *pattern,
      const char *by);

/* Remove leading whitespaces */
char *string_trim_whitespace_left(char *const s);

/* Remove trailing whitespaces */
char *string_trim_whitespace_right(char *const s);

/* Remove leading and trailing whitespaces */
char *string_trim_whitespace(char *const s);

/* max_lines == 0 means no limit */
char *word_wrap(char *buffer, const char *string,
      int line_width, bool unicode, unsigned max_lines);

/* Splits string into tokens seperated by 'delim'
 * > Returned token string must be free()'d
 * > Returns NULL if token is not found
 * > After each call, 'str' is set to the position after the
 *   last found token
 * > Tokens *include* empty strings
 * Usage example:
 *    char *str      = "1,2,3,4,5,6,7,,,10,";
 *    char **str_ptr = &str;
 *    char *token    = NULL;
 *    while((token = string_tokenize(str_ptr, ",")))
 *    {
 *        printf("%s\n", token);
 *        free(token);
 *        token = NULL;
 *    }
 */
char* string_tokenize(char **str, const char *delim);

/* Removes every instance of character 'c' from 'str' */
void string_remove_all_chars(char *str, char c);

/* Converts string to unsigned integer.
 * Returns 0 if string is invalid  */
unsigned string_to_unsigned(const char *str);

RETRO_END_DECLS

#endif

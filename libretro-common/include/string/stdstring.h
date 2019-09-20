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

#define string_add_pair_open(s, size)     strlcat((s), " (", (size))
#define string_add_pair_close(s, size)    strlcat((s), ")",  (size))
#define string_add_bracket_open(s, size)  strlcat((s), "{",  (size))
#define string_add_bracket_close(s, size) strlcat((s), "}",  (size))
#define string_add_single_quote(s, size)  strlcat((s), "'",  (size))
#define string_add_quote(s, size)         strlcat((s), "\"",  (size))
#define string_add_colon(s, size)         strlcat((s), ":",  (size))
#define string_add_glob_open(s, size)     strlcat((s), "glob('*",  (size))
#define string_add_glob_close(s, size)    strlcat((s), "*')",  (size))

#define STRLCPY_CONST(buf, str) \
   do { size_t i; for (i = 0; i < sizeof(str); i++) (buf)[i] = (str)[i]; } while (0)

#define STRLCAT_CONST(buf, strlcpy_ret, str, buf_size) \
   STRLCPY_CONST((buf) + MIN((strlcpy_ret), (buf_size)-1 - STRLEN_CONST((str))), (str))

#define STRLCAT_CONST_INCR(buf, strlcpy_ret, str, buf_size) \
   STRLCAT_CONST(buf, strlcpy_ret, str, buf_size); \
   (strlcpy_ret) += STRLEN_CONST((str))

#define string_add_alpha_fast(s, alpha, size) \
   (s)[(size)]   = (alpha); \
   (s)[(size)+1] = '\0'

#define string_add_alpha_2_fast(s, str, size) \
   string_add_alpha_fast(s, str[0], size); \
   string_add_alpha_fast(s, str[1], size+1)

#define string_add_alpha_3_fast(s, str, size) \
   string_add_alpha_2_fast(s, str, size); \
   string_add_alpha_fast(s, str[2], size+2)

#define string_add_alpha_4_fast(s, str, size) \
   string_add_alpha_3_fast(s, str,  size); \
   string_add_alpha_fast(s, str[3], size+3)

#define string_add_alpha_5_fast(s, str, size) \
   string_add_alpha_4_fast(s, str,  size); \
   string_add_alpha_fast(s, str[4], size+4)

#define string_add_alpha_6_fast(s, str, size) \
   string_add_alpha_5_fast(s, str,  size); \
   string_add_alpha_fast(s, str[5], size+5)

#define string_add_alpha_7_fast(s, str, size) \
   string_add_alpha_6_fast(s, str,  size); \
   string_add_alpha_fast(s, str[6], size+6)

#define string_add_alpha_8_fast(s, str, size) \
   string_add_alpha_7_fast(s, str,  size); \
   string_add_alpha_fast(s, str[7], size+7)

#define string_add_alpha_9_fast(s, str, size) \
   string_add_alpha_8_fast(s, str,  size); \
   string_add_alpha_fast(s, str[8], size+8)

#define string_add_alpha_10_fast(s, str, size) \
   string_add_alpha_9_fast(s, str,  size); \
   string_add_alpha_fast(s, str[9], size+9)

#define string_add_alpha_11_fast(s, str, size) \
   string_add_alpha_10_fast(s, str,  size); \
   string_add_alpha_fast(s, str[10], size+10)

#define string_add_alpha_12_fast(s, str, size) \
   string_add_alpha_11_fast(s, str,  size); \
   string_add_alpha_fast(s, str[11], size+11)

#define string_add_alpha_13_fast(s, str, size) \
   string_add_alpha_12_fast(s, str,  size); \
   string_add_alpha_fast(s, str[12], size+12)

#define string_add_alpha_14_fast(s, str, size) \
   string_add_alpha_13_fast(s, str,  size); \
   string_add_alpha_fast(s, str[13], size+13)

#define string_add_alpha_15_fast(s, str, size) \
   string_add_alpha_14_fast(s, str,  size); \
   string_add_alpha_fast(s, str[14], size+14)

#define string_add_alpha_16_fast(s, str, size) \
   string_add_alpha_15_fast(s, str,  size); \
   string_add_alpha_fast(s, str[15], size+15)

#define string_add_backslash_fast(s, size) \
   (s)[(size)]   = '/'; \
   (s)[(size)+1] = '\0'

#define string_add_colon_fast(s, size) \
   (s)[(size)]   = ':'; \
   (s)[(size)+1] = '\0'

#define string_add_star_fast(s, size) \
   (s)[(size)]   = '*'; \
   (s)[(size)+1] = '\0'

#define string_add_dot_fast(s, size) \
   (s)[(size)]   = '.'; \
   (s)[(size)+1] = '\0'

#define string_add_space_fast(s, size) \
   (s)[(size)]   = ' '; \
   (s)[(size)+1] = '\0'

#define string_add_vertical_bar_fast(s, size) \
   (s)[(size)]   = '|'; \
   (s)[(size)+1] = '\0'

#define string_add_pair_open_fast(s, size) \
   (s)[(size)]   = '('; \
   (s)[(size)+1] = '\0'

#define string_add_pair_close_fast(s, size) \
   (s)[(size)]   = ')'; \
   (s)[(size)+1] = '\0'

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
unsigned string_to_unsigned(char *str);

RETRO_END_DECLS

#endif

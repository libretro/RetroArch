/* Copyright  (C) 2010-2020 The RetroArch team
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

#define STRLEN_CONST(x)                   ((sizeof((x))-1))

#define strcpy_literal(a, b)              strcpy(a, b)

#define string_is_not_equal(a, b)         !string_is_equal((a), (b))

#define string_is_not_equal_fast(a, b, size) (memcmp(a, b, size) != 0)
#define string_is_equal_fast(a, b, size)     (memcmp(a, b, size) == 0)

#define TOLOWER(c)   (c |  (lr_char_props[c] & 0x20))
#define TOUPPER(c)   (c & ~(lr_char_props[c] & 0x20))

/* C standard says \f \v are space, but this one disagrees */
#define ISSPACE(c)   (lr_char_props[c]  & 0x80) 

#define ISDIGIT(c)   (lr_char_props[c]  & 0x40)
#define ISALPHA(c)   (lr_char_props[c]  & 0x20)
#define ISLOWER(c)   (lr_char_props[c]  & 0x04)
#define ISUPPER(c)   (lr_char_props[c]  & 0x02)
#define ISALNUM(c)   (lr_char_props[c]  & 0x60)
#define ISUALPHA(c)  (lr_char_props[c]  & 0x28)
#define ISUALNUM(c)  (lr_char_props[c]  & 0x68)
#define IS_XDIGIT(c) (lr_char_props[c]  & 0x01)

/* Deprecated alias, all callers should use string_is_equal_case_insensitive instead */
#define string_is_equal_noncase string_is_equal_case_insensitive

static INLINE bool string_is_empty(const char *data)
{
   return !data || (*data == '\0');
}

static INLINE bool string_is_equal(const char *a, const char *b)
{
   return (a && b) ? !strcmp(a, b) : false;
}

static INLINE bool string_starts_with_size(const char *str, const char *prefix,
      size_t size)
{
   return (str && prefix) ? !strncmp(prefix, str, size) : false;
}

static INLINE bool string_starts_with(const char *str, const char *prefix)
{
   return (str && prefix) ? !strncmp(prefix, str, strlen(prefix)) : false;
}

static INLINE bool string_ends_with_size(const char *str, const char *suffix,
      size_t str_len, size_t suffix_len)
{
   return (str_len < suffix_len) ? false :
         !memcmp(suffix, str + (str_len - suffix_len), suffix_len);
}

static INLINE bool string_ends_with(const char *str, const char *suffix)
{
   if (!str || !suffix)
      return false;
   return string_ends_with_size(str, suffix, strlen(str), strlen(suffix));
}

/* Returns the length of 'str' (c.f. strlen()), but only
 * checks the first 'size' characters
 * - If 'str' is NULL, returns 0
 * - If 'str' is not NULL and no '\0' character is found
 *   in the first 'size' characters, returns 'size' */
static INLINE size_t strlen_size(const char *str, size_t size)
{
   size_t i = 0;
   if (str)
      while (i < size && str[i]) i++;
   return i;
}


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
 *    while ((token = string_tokenize(str_ptr, ",")))
 *    {
 *        printf("%s\n", token);
 *        free(token);
 *        token = NULL;
 *    }
 */
char* string_tokenize(char **str, const char *delim);

/* Removes every instance of character 'c' from 'str' */
void string_remove_all_chars(char *str, char c);

/* Replaces every instance of character 'find' in 'str'
 * with character 'replace' */
void string_replace_all_chars(char *str, char find, char replace);

/* Converts string to unsigned integer.
 * Returns 0 if string is invalid  */
unsigned string_to_unsigned(const char *str);

/* Converts hexadecimal string to unsigned integer.
 * Handles optional leading '0x'.
 * Returns 0 if string is invalid  */
unsigned string_hex_to_unsigned(const char *str);

char *string_init(const char *src);

void string_set(char **string, const char *src);

extern const unsigned char lr_char_props[256];

RETRO_END_DECLS

#endif

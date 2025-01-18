/* Copyright  (C) 2010-2021 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rjson.c).
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

/* The parser is based on Public Domain JSON Parser for C by Christopher Wellons - https://github.com/skeeto/pdjson */

#include <stdio.h>  /* snprintf, vsnprintf */
#include <stdarg.h> /* va_list */
#include <string.h> /* memcpy */
#include <stdint.h> /* int64_t */
#include <stdlib.h> /* malloc, realloc, atof, atoi */

#include <formats/rjson.h>
#include <compat/posix_string.h>
#include <streams/interface_stream.h>
#include <streams/file_stream.h>

struct _rjson_stack { enum rjson_type type; size_t count; };

struct rjson
{
   /* Order of the top few struct elements have an impact on performance */
   /* Place most frequently accessed things on top */
   const unsigned char *input_p;
   struct _rjson_stack *stack_top;
   const unsigned char *input_end;
   const unsigned char* source_column_p;
   size_t source_line;

   char *string, *string_pass_through;
   size_t string_len, string_cap;

   struct _rjson_stack inline_stack[10];
   struct _rjson_stack *stack;

   rjson_io_t io;
   void *user_data;

   unsigned int stack_cap, stack_max;
   int input_len;

   char option_flags;
   char decimal_sep;
   char error_text[80];
   char inline_string[512];

   /* Must be at the end of the struct, can be allocated with custom size */
   unsigned char input_buf[512];
};

enum _rjson_token
{
   _rJSON_TOK_WHITESPACE, _rJSON_TOK_NEWLINE, _rJSON_TOK_OPTIONAL_SKIP,
   _rJSON_TOK_OBJECT, _rJSON_TOK_ARRAY, _rJSON_TOK_STRING, _rJSON_TOK_NUMBER,
   _rJSON_TOK_TRUE, _rJSON_TOK_FALSE, _rJSON_TOK_NULL,
   _rJSON_TOK_OBJECT_END, _rJSON_TOK_ARRAY_END, _rJSON_TOK_COLON,
   _rJSON_TOK_COMMA, _rJSON_TOK_ERROR, _rJSON_TOK_EOF, _rJSON_TOKCOUNT
};

/* The used char type is int and not short for better performance */
typedef unsigned int _rjson_char_t;
#define _rJSON_EOF ((_rjson_char_t)256)

/* Compiler branching hint for expression with high probability
 * Explicitly only have likely (and no unlikely) because compilers
 * that don't support it expect likely branches to come first. */
#if defined(__GNUC__) || defined(__clang__)
#define _rJSON_LIKELY(x) __builtin_expect(!!(x), 1)
#else
#define _rJSON_LIKELY(x) (x)
#endif

/* These 3 error functions return RJSON_ERROR for convenience */
static enum rjson_type _rjson_error(rjson_t *json, const char *fmt, ...)
{
   va_list ap;
   if (json->stack_top->type == RJSON_ERROR)
      return RJSON_ERROR;
   json->stack_top->type = RJSON_ERROR;
   va_start(ap, fmt);
   vsnprintf(json->error_text, sizeof(json->error_text), fmt, ap);
   va_end(ap);
   return RJSON_ERROR;
}

static enum rjson_type _rjson_error_char(rjson_t *json,
      const char *fmt, _rjson_char_t chr)
{
   char buf[16];
   if (json->stack_top->type == RJSON_ERROR)
      return RJSON_ERROR;
   snprintf(buf, sizeof(buf),
         (chr == _rJSON_EOF ? "end of stream" :
         (chr >= ' ' && chr <= '~' ? "'%c'" : "byte 0x%02X")), chr);
   return _rjson_error(json, fmt, buf);
}

static enum rjson_type _rjson_error_token(rjson_t *json,
   const char *fmt, enum _rjson_token tok)
{
   return _rjson_error_char(json, fmt,
         (tok == _rJSON_TOK_EOF ? _rJSON_EOF : json->input_p[-1]));
}

static bool _rjson_io_input(rjson_t *json)
{
   if (json->input_end == json->input_buf)
      return false;
   json->source_column_p -= (json->input_end - json->input_buf);
   json->input_p = json->input_buf;
   json->input_end = json->input_buf +
         json->io(json->input_buf, json->input_len, json->user_data);
   if (json->input_end < json->input_buf)
   {
      _rjson_error(json, "input stream read error");
      json->input_end = json->input_buf;
   }
   return (json->input_end != json->input_p);
}

static bool _rjson_grow_string(rjson_t *json)
{
   char *string;
   size_t new_string_cap = json->string_cap * 2;
   if (json->string != json->inline_string)
      string             = (char*)realloc(json->string, new_string_cap);
   else if ((string      = (char*)malloc(new_string_cap)) != NULL)
      memcpy(string, json->inline_string, sizeof(json->inline_string));
   if (!string)
   {
      _rjson_error(json, "out of memory");
      return false;
   }
   json->string_cap      = new_string_cap;
   json->string          = string;
   return true;
}

static INLINE bool _rjson_pushchar(rjson_t *json, _rjson_char_t c)
{
   json->string[json->string_len++] = (char)c;
   return (json->string_len != json->string_cap || _rjson_grow_string(json));
}

static INLINE bool _rjson_pushchars(rjson_t *json,
      const unsigned char *from, const unsigned char *to)
{
   unsigned char* string;
   size_t _len    = json->string_len;
   size_t new_len = _len + (to - from);
   while (new_len >= json->string_cap)
      if (!_rjson_grow_string(json))
         return false;
   string = (unsigned char *)json->string;
   while (_len != new_len)
      string[_len++] = *(from++);
   json->string_len = new_len;
   return true;
}

static INLINE _rjson_char_t _rjson_char_get(rjson_t *json)
{
   return (json->input_p != json->input_end || _rjson_io_input(json)
        ? *json->input_p++ : _rJSON_EOF);
}

static unsigned int _rjson_get_unicode_cp(rjson_t *json)
{
   unsigned int cp = 0, shift = 16;
   for (;;)
   {
      _rjson_char_t c = _rjson_char_get(json);
      switch (c)
      {
         case '0': case '1': case '2': case '3': case '4':
         case '5': case '6': case '7': case '8': case '9':
            c -= '0';
            break;
         case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
            c -= ('a' - 10);
            break;
         case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            c -= ('A' - 10);
            break;
         case _rJSON_EOF:
            _rjson_error(json, "unterminated string literal in Unicode");
            return (unsigned int)-1;
         default:
            _rjson_error_char(json, "invalid Unicode escape hexadecimal %s", c);
            return (unsigned int)-1;
      }
      shift -= 4;
      cp |= ((unsigned int)c << shift);
      if (!shift)
         return cp;
   }
}

static bool _rjson_read_unicode(rjson_t *json)
{
   #define _rJSON_READ_UNICODE_REPLACE_OR_IGNORE \
      if (json->option_flags & (RJSON_OPTION_IGNORE_INVALID_ENCODING \
            | RJSON_OPTION_REPLACE_INVALID_ENCODING)) goto replace_or_ignore;

   unsigned int cp;

   if ((cp = _rjson_get_unicode_cp(json)) == (unsigned int)-1)
      return false;

   if (cp >= 0xd800 && cp <= 0xdbff)
   {
      /* This is the high portion of a surrogate pair; we need to read the
       * lower portion to get the codepoint */
      unsigned int l, h = cp;

      _rjson_char_t c = _rjson_char_get(json);
      if (c == _rJSON_EOF)
      {
         _rjson_error(json, "unterminated string literal in Unicode");
         return false;
      }
      if (c != '\\')
      {
         _rjson_error_char(json, "invalid continuation %s"
               " for surrogate pair, expected '\\'", c);
         return false;
      }

      c = _rjson_char_get(json);
      if (c == _rJSON_EOF)
      {
         _rjson_error(json, "unterminated string literal in Unicode");
         return false;
      }
      if (c != 'u')
      {
         _rjson_error_char(json, "invalid continuation %s"
               " for surrogate pair, expected 'u'", c);
         return false;
      }
      if ((l = _rjson_get_unicode_cp(json)) == (unsigned int)-1)
         return false;
      if (l < 0xdc00 || l > 0xdfff)
      {
         _rJSON_READ_UNICODE_REPLACE_OR_IGNORE
         _rjson_error(json, "surrogate pair continuation \\u%04x out "
            "of range (dc00-dfff)", l);
         return false;
      }
      cp = ((h - 0xd800) * 0x400) + ((l - 0xdc00) + 0x10000);
   }
   else if (cp >= 0xdc00 && cp <= 0xdfff)
   {
      _rJSON_READ_UNICODE_REPLACE_OR_IGNORE
      _rjson_error(json, "dangling surrogate \\u%04x", cp);
      return false;
   }

   if (cp < 0x80UL)
      return _rjson_pushchar(json, cp);

   if (cp < 0x0800UL)
      return (_rjson_pushchar(json, (cp >> 6 & 0x1F) | 0xC0) &&
              _rjson_pushchar(json, (cp >> 0 & 0x3F) | 0x80));

   if (cp < 0x010000UL)
   {
      if (cp >= 0xd800 && cp <= 0xdfff)
      {
         _rJSON_READ_UNICODE_REPLACE_OR_IGNORE
         _rjson_error(json, "invalid codepoint %04x", cp);
         return false;
      }
      return (_rjson_pushchar(json, (cp >> 12 & 0x0F) | 0xE0) &&
              _rjson_pushchar(json, (cp >>  6 & 0x3F) | 0x80) &&
              _rjson_pushchar(json, (cp >>  0 & 0x3F) | 0x80));
   }
   if (cp < 0x110000UL)
      return (_rjson_pushchar(json, (cp >> 18 & 0x07) | 0xF0) &&
              _rjson_pushchar(json, (cp >> 12 & 0x3F) | 0x80) &&
              _rjson_pushchar(json, (cp >>  6 & 0x3F) | 0x80) &&
              _rjson_pushchar(json, (cp >>  0 & 0x3F) | 0x80));

   _rJSON_READ_UNICODE_REPLACE_OR_IGNORE
   _rjson_error(json, "unable to encode %04x as UTF-8", cp);
   return false;

replace_or_ignore:
   return ((json->option_flags & RJSON_OPTION_IGNORE_INVALID_ENCODING) ||
         _rjson_pushchar(json, '?'));
   #undef _rJSON_READ_UNICODE_REPLACE_OR_IGNORE
}

static bool _rjson_validate_utf8(rjson_t *json)
{
   unsigned char first, c;
   unsigned char *p;
   unsigned char *from = (unsigned char *)
         (json->string_pass_through ? json->string_pass_through : json->string);
   unsigned char *to = from + json->string_len;

   if (json->option_flags & RJSON_OPTION_IGNORE_INVALID_ENCODING)
      return true;

   for (;;)
   {
      if (from == to)
         return true;
      first = *from;
      if (first <= 0x7F) /* ASCII */
      {
         from++;
         continue;
      }
      p = from;
      /* Continuation or overlong encoding of an ASCII byte */
      if (first <= 0xC1)
         goto invalid_utf8;
      if (first <= 0xDF)
      {
         if ((from = p + 2) > to)
            goto invalid_utf8;
continue_length_2:
         c = p[1];
         switch (first)
         {
            case 0xE0:
               c = (c < 0xA0 || c > 0xBF);
               break;
            case 0xED:
               c = (c < 0x80 || c > 0x9F);
               break;
            case 0xF0:
               c = (c < 0x90 || c > 0xBF);
               break;
            case 0xF4:
               c = (c < 0x80 || c > 0x8F);
               break;
            default:
               c = (c < 0x80 || c > 0xBF);
               break;
         }
         if (c)
            goto invalid_utf8;
      }
      else if (first <= 0xEF)
      {
         if ((from = p + 3) > to)
            goto invalid_utf8;
continue_length_3:
         if ((c = p[2]) < 0x80 || c > 0xBF)
            goto invalid_utf8;
         goto continue_length_2;
      }
      else if (first <= 0xF4)
      {
         if ((from = p + 4) > to)
            goto invalid_utf8;
         if ((c = p[3]) < 0x80 || c > 0xBF)
            goto invalid_utf8;
         goto continue_length_3;
      }
      else
         goto invalid_utf8; /* length 5 or 6 or invalid UTF-8 */
      continue;
invalid_utf8:
      if (!(json->option_flags & RJSON_OPTION_REPLACE_INVALID_ENCODING))
      {
         _rjson_error(json, "invalid UTF-8 character in string");
         return false;
      }
      from    = p;
      *from++ = '?';
      while (from != to && (*from & 0x80))
         *from++ = '?';
   }
}

static enum rjson_type _rjson_read_string(rjson_t *json)
{
   const unsigned char *p    = json->input_p, *raw = p;
   const unsigned char *end  = json->input_end;
   unsigned char utf8mask    = 0;
   json->string_pass_through = NULL;
   json->string_len          = 0;

   for (;;)
   {
      if (_rJSON_LIKELY(p != end))
      {
         unsigned char c = *p;
         if (_rJSON_LIKELY(c != '"' && c != '\\' && c >= 0x20))
         {
            /* handle most common case first, it's faster */
            utf8mask |= c;
            p++;
         }
         else if (c == '"')
         {
            json->input_p = p + 1;
            if (json->string_len == 0 && p + 1 != end)
            {
               /* raw string fully inside input buffer, pass through */
               json->string_len          = p - raw;
               json->string_pass_through = (char*)raw;
            }
            else if (raw != p && !_rjson_pushchars(json, raw, p)) /* OOM */
               return RJSON_ERROR;
            /* Contains invalid UTF-8 byte sequences */
            if ((utf8mask & 0x80) && !_rjson_validate_utf8(json))
               return RJSON_ERROR;
            return RJSON_STRING;
         }
         else if (c == '\\')
         {
            _rjson_char_t esc;
            if (raw != p)
            {
               /* Can't pass through string with escapes, use string buffer */
               if (!_rjson_pushchars(json, raw, p))
                  return RJSON_ERROR;
            }
            json->input_p = p + 1;
            esc = _rjson_char_get(json);
            switch (esc)
            {
               case 'u':
                  if (!_rjson_read_unicode(json))
                     return RJSON_ERROR;
                  break;

               case 'b':
                  esc = '\b';
                  goto escape_pushchar;
               case 'f':
                  esc = '\f';
                  goto escape_pushchar;
               case 'n':
                  esc = '\n';
                  goto escape_pushchar;
               case 'r':
                  if (!(json->option_flags & RJSON_OPTION_IGNORE_STRING_CARRIAGE_RETURN))
                  {
                     esc = '\r';
                     goto escape_pushchar;
                  }
                  break;
               case 't':
                  esc = '\t';
                  goto escape_pushchar;
               case '/':
               case '"':
               case '\\':
escape_pushchar:
                  if (!_rjson_pushchar(json, esc))
                     return RJSON_ERROR;
                  break;

               case _rJSON_EOF:
                  return _rjson_error(json, "unterminated string literal in escape");

               default:
                  return _rjson_error_char(json, "invalid escaped %s", esc);
            }
            raw = p = json->input_p;
            end     = json->input_end;
         }
         else if (!(json->option_flags & RJSON_OPTION_ALLOW_UNESCAPED_CONTROL_CHARACTERS))
            return _rjson_error_char(json, "unescaped control character %s in string", c);
         else
            p++;
      }
      else
      {
         if (raw != p)
         {
            /* not fully inside input buffer, copy to string buffer */
            if (!_rjson_pushchars(json, raw, p))
               return RJSON_ERROR;
         }
         if (!_rjson_io_input(json))
            return _rjson_error(json, "unterminated string literal");
         raw = p = json->input_p;
         end     = json->input_end;
      }
   }
}

static enum rjson_type _rjson_read_number(rjson_t *json)
{
   const unsigned char *p     = json->input_p - 1;
   const unsigned char *end   = json->input_end;
   const unsigned char *start = p;

   json->string_len = 0;
   json->string_pass_through = NULL;
   for (;;)
   {
      if (_rJSON_LIKELY(p != end))
      {
         switch (*p++)
         {
            case '+': case '-': case '.':
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
            case 'E': case 'e':
               continue;
         }
         p--;
         json->input_p = p;
         if (!_rjson_pushchars(json, start, p))
            return RJSON_ERROR; /* out of memory */
         break;
      }
      else
      {
         /* number sequences are always copied to the string buffer */
         if (!_rjson_pushchars(json, start, p))
            return RJSON_ERROR;
         if (!_rjson_io_input(json))
         {
            /* EOF here is not an error for a number */
            json->input_p = json->input_end;
            break;
         }
         start = p = json->input_p;
         end = json->input_end;
      }
   }

   p = (const unsigned char *)json->string;
   end = (p + json->string_len);

   /* validate json number */
   if (*p == '-' && ++p == end)
      goto invalid_number;
   if (*p == '0')
   {
      if (++p == end)
         return RJSON_NUMBER;
   }
   else
   {
      if (*p < '1' || *p > '9')
         goto invalid_number;
      do
      {
         if (++p == end)
            return RJSON_NUMBER;
      }
      while (*p >= '0' && *p <= '9');
   }
   if (*p == '.')
   {
      if (++p == end)
         goto invalid_number;
      if (*p < '0' || *p > '9')
         goto invalid_number;
      do
      {
         if (++p == end)
            return RJSON_NUMBER;
      }
      while (*p >= '0' && *p <= '9');
   }
   if (((*p)|0x20) == 'e')
   {
      if (++p == end)
         goto invalid_number;
      if ((*p == '-' || *p == '+') && ++p == end)
         goto invalid_number;
      if (*p < '0' || *p > '9')
         goto invalid_number;
      do
      {
         if (++p == end)
            return RJSON_NUMBER;
      }
      while (*p >= '0' && *p <= '9');
   }
invalid_number:
   return _rjson_error_char(json, "unexpected %s in number",
         (p == json->input_end ? _rJSON_EOF : p[p == end ? -1 : 0]));
}

static enum rjson_type _rjson_push_stack(rjson_t *json, enum _rjson_token t)
{
   if (json->stack_top + 1 == json->stack + json->stack_cap)
   {
      /* reached allocated stack size, either reallocate or abort */
      unsigned int new_stack_cap;
      struct _rjson_stack *new_stack;
      size_t stack_alloc;
      if (json->stack_cap == json->stack_max)
         return _rjson_error(json, "maximum depth of nesting reached");

      new_stack_cap = json->stack_cap + 4;
      if (new_stack_cap > json->stack_max)
         new_stack_cap = json->stack_max;
      stack_alloc = new_stack_cap * sizeof(struct _rjson_stack);
      if (json->stack != json->inline_stack)
         new_stack = (struct _rjson_stack *)realloc(json->stack, stack_alloc);
      else if ((new_stack = (struct _rjson_stack*)malloc(stack_alloc)) != NULL)
         memcpy(new_stack, json->inline_stack, sizeof(json->inline_stack));
      if (!new_stack)
         return _rjson_error(json, "out of memory");

      json->stack     = new_stack;
      json->stack_top = new_stack + json->stack_cap - 1;
      json->stack_cap = new_stack_cap;
   }
   json->stack_top++;
   json->stack_top->count = 0;
   return (json->stack_top->type =
            (t == _rJSON_TOK_ARRAY ? RJSON_ARRAY : RJSON_OBJECT));
}

static enum rjson_type _rjson_read_name(rjson_t *json, const char *pattern, enum rjson_type type)
{
   _rjson_char_t c;
   const char *p;
   for (p = pattern; *p; p++)
   {
      if ((_rjson_char_t)*p != (c = _rjson_char_get(json)))
         return _rjson_error_char(json, "unexpected %s in value", c);
   }
   return type;
}

static bool _rjson_optional_skip(rjson_t *json, const unsigned char **p, const unsigned char **end)
{
   unsigned char c, skip = (*p)[-1];
   int state = 0;

   if (skip == '/' && !(json->option_flags & RJSON_OPTION_ALLOW_COMMENTS))
      return false;

   if (     skip == 0xEF && (!(json->option_flags & RJSON_OPTION_ALLOW_UTF8BOM)
         || json->source_line != 1 || json->source_column_p != json->input_p))
      return false;

   for (;;)
   {
      if (*p == *end)
      {
         if (!_rjson_io_input(json))
         {
            _rjson_error(json, "unfinished %s",
                  (skip == '/' ? "comment" : "utf8 byte order mark"));
            break;
         }
         *p   = json->input_p;
         *end = json->input_end;
      }
      c = *(*p)++;
      if (skip == '/')
      {
         if      (state == 0 && c == '/')
            state = 1;
         else if (state == 0 && c == '*')
            state = 2;
         else if (state == 0)
            break;
         else if (state == 1 && c == '\n')
            return true;
         else if (state == 2 && c == '*')
            state = 3;
         else if (state == 3 && c == '/')
            return true;
         else if (state == 3 && c != '*')
            state = 2;
      }
      else if (skip == 0xEF)
      {
         /* Silence warning - state being set never used */
         if      (state == 0 && c == 0xBB)
            state = 1;
         else if (state == 1 && c == 0xBF)
            return true;
         else
            break;
      }
   }
   return false;
}

enum rjson_type rjson_next(rjson_t *json)
{
   unsigned char tok;
   struct _rjson_stack *stack = json->stack_top;
   const unsigned char *p     = json->input_p;
   const unsigned char *end   = json->input_end;
   unsigned char passed_token = false;

   /* JSON token look-up-table */
   static const unsigned char token_lut[256] =
   {
      #define i _rJSON_TOK_ERROR
      /*   0 | 0x00 |   */ i,i,i,i,i,i,i,i,i,
      /*   9 | 0x09 |\t */ _rJSON_TOK_WHITESPACE,
      /*  10 | 0x0A |\n */ _rJSON_TOK_NEWLINE, i,i,
      /*  13 | 0x0D |\r */ _rJSON_TOK_WHITESPACE, i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,
      /*  32 | 0x20 |   */ _rJSON_TOK_WHITESPACE, i,
      /*  34 | 0x22 | " */ _rJSON_TOK_STRING, i,i,i,i,i,i,i,i,i,
      /*  44 | 0x2C | , */ _rJSON_TOK_COMMA,
      /*  45 | 0x2D | - */ _rJSON_TOK_NUMBER, i,
      /*  47 | 0x2F | / */ _rJSON_TOK_OPTIONAL_SKIP,
      /*  48 | 0x30 | 0 */ _rJSON_TOK_NUMBER, _rJSON_TOK_NUMBER, _rJSON_TOK_NUMBER, _rJSON_TOK_NUMBER, _rJSON_TOK_NUMBER,
      /*  53 | 0x35 | 5 */ _rJSON_TOK_NUMBER, _rJSON_TOK_NUMBER, _rJSON_TOK_NUMBER, _rJSON_TOK_NUMBER, _rJSON_TOK_NUMBER,
      /*  58 | 0x3A | : */ _rJSON_TOK_COLON, i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,
      /*  91 | 0x5B | [ */ _rJSON_TOK_ARRAY, i,
      /*  93 | 0x5D | ] */ _rJSON_TOK_ARRAY_END, i,i,i,i,i,i,i,i,
      /* 102 | 0x66 | f */ _rJSON_TOK_FALSE, i,i,i,i,i,i,i,
      /* 110 | 0x6E | n */ _rJSON_TOK_NULL, i,i,i,i,i,
      /* 116 | 0x74 | t */ _rJSON_TOK_TRUE, i,i,i,i,i,i,
      /* 123 | 0x7B | { */ _rJSON_TOK_OBJECT, i,
      /* 125 | 0x7D | } */ _rJSON_TOK_OBJECT_END,
      /* 126 | 0x7E | ~ */ i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,
      /* 164 | 0xA4 |   */ i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,
      /* 202 | 0xCA |   */ i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,
      /* 239 | 0xEF |   */ _rJSON_TOK_OPTIONAL_SKIP, i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i
      #undef i
   };

   if (_rJSON_LIKELY(stack->type != RJSON_ERROR))
   {
      for (;;)
      {
         if (_rJSON_LIKELY(p != end))
         {
            tok = token_lut[*p++];
            if (_rJSON_LIKELY(tok > _rJSON_TOK_OPTIONAL_SKIP))
            {
               /* Actual JSON token, process below */
            }
            else if (_rJSON_LIKELY(tok == _rJSON_TOK_WHITESPACE))
               continue;
            else if (tok == _rJSON_TOK_NEWLINE)
            {
               json->source_line++;
               json->source_column_p = p;
               continue;
            }
            else if (tok == _rJSON_TOK_OPTIONAL_SKIP)
            {
               if (_rjson_optional_skip(json, &p, &end))
                  continue;
            }
         }
         else if (_rJSON_LIKELY(_rjson_io_input(json)))
         {
            p   = json->input_p;
            end = json->input_end;
            continue;
         }
         else
         {
            p   = json->input_end;
            tok = _rJSON_TOK_EOF;
         }

         if (stack->type == RJSON_OBJECT)
         {
            if (stack->count & 1)
            {
               /* Expecting colon followed by value. */
               if (passed_token)
                  goto read_value;
               if (_rJSON_LIKELY(tok == _rJSON_TOK_COLON))
               {
                  passed_token = true;
                  continue;
               }
               json->input_p = p;
               return _rjson_error_token(json,
                     "expected ':' not %s after member name", (enum _rjson_token)tok);
            }
            if (passed_token)
            {
               if (_rJSON_LIKELY(tok == _rJSON_TOK_STRING))
                  goto read_value;
               json->input_p = p;
               return _rjson_error(json, "expected member name after ','");
            }
            if (tok == _rJSON_TOK_OBJECT_END)
            {
               json->input_p = p;
               json->stack_top--;
               return RJSON_OBJECT_END;
            }
            if (stack->count == 0)
            {
               /* No member name/value pairs yet. */
               if (_rJSON_LIKELY(tok == _rJSON_TOK_STRING))
                  goto read_value;
               json->input_p = p;
               return _rjson_error(json, "expected member name or '}'");
            }
            /* Expecting comma followed by member name. */
            if (_rJSON_LIKELY(tok == _rJSON_TOK_COMMA))
            {
               passed_token = true;
               continue;
            }
            json->input_p = p;
            return _rjson_error_token(json,
                  "expected ',' or '}' not %s after member value", (enum _rjson_token)tok);
         }
         else if (stack->type == RJSON_ARRAY)
         {
            if (passed_token)
               goto read_value;
            if (tok == _rJSON_TOK_ARRAY_END)
            {
               json->input_p = p;
               json->stack_top--;
               return RJSON_ARRAY_END;
            }
            if (stack->count == 0)
               goto read_value;
            if (_rJSON_LIKELY(tok == _rJSON_TOK_COMMA))
            {
               passed_token = true;
               continue;
            }
            json->input_p = p;
            return _rjson_error_token(json,
                  "expected ',' or ']' not %s in array", (enum _rjson_token)tok);
         }
         else
         {
            if (_rJSON_LIKELY(!stack->count && tok != _rJSON_TOK_EOF))
               goto read_value;
            json->input_p = p;
            if (!stack->count)
               return _rjson_error(json, "reached end without any data");
            if (tok == _rJSON_TOK_EOF)
               return RJSON_DONE;
            if (!(json->option_flags & RJSON_OPTION_ALLOW_TRAILING_DATA))
               return _rjson_error_token(json,
                     "expected end of stream instead of %s", (enum _rjson_token)tok);
            json->input_p--;
            return RJSON_DONE;
         }

         /* read value for current token */
         read_value:
         json->input_p = p;
         stack->count++;
         /* This is optimal when there are many strings, otherwise a switch statement
          * or a function pointer table is better (depending on compiler/cpu) */
         if      (tok == _rJSON_TOK_STRING)
            return _rjson_read_string(json);
         else if (tok == _rJSON_TOK_NUMBER)
            return _rjson_read_number(json);
         else if (tok == _rJSON_TOK_OBJECT)
            return _rjson_push_stack(json, _rJSON_TOK_OBJECT);
         else if (tok == _rJSON_TOK_ARRAY)
            return _rjson_push_stack(json, _rJSON_TOK_ARRAY);
         else if (tok == _rJSON_TOK_TRUE)
            return _rjson_read_name(json, "rue", RJSON_TRUE);
         else if (tok == _rJSON_TOK_FALSE)
            return _rjson_read_name(json, "alse", RJSON_FALSE);
         else if (tok == _rJSON_TOK_NULL)
            return _rjson_read_name(json, "ull", RJSON_NULL);
         else return _rjson_error_token(json,
               "unexpected %s in value", (enum _rjson_token)tok);
      }
   }
   return RJSON_ERROR;
}

void _rjson_setup(rjson_t *json, rjson_io_t io, void *user_data, int input_len)
{
   json->io                  = io;
   json->user_data           = user_data;
   json->input_len           = input_len;
   json->input_p             = json->input_end = json->input_buf + input_len;

   json->stack               = json->inline_stack;
   json->stack_top           = json->stack;
   json->stack_top->type     = RJSON_DONE;
   json->stack_top->count    = 0;
   json->stack_cap           = (unsigned int)(sizeof(json->inline_stack) / sizeof(json->inline_stack[0]));
   json->stack_max           = (unsigned int)50;

   json->string              = json->inline_string;
   json->string_pass_through = NULL;
   json->string_len          = 0;
   json->string_cap          = sizeof(json->inline_string);

   json->source_line         = 1;
   json->source_column_p     = json->input_p;
   json->option_flags        = 0;
   json->decimal_sep         = 0;
}

rjson_t *rjson_open_user(rjson_io_t io, void *user_data, int io_block_size)
{
   rjson_t* json = (rjson_t*)malloc(
         sizeof(rjson_t) - sizeof(((rjson_t*)0)->input_buf) + io_block_size);
   if (json) _rjson_setup(json, io, user_data, io_block_size);
   return json;
}

static int _rjson_buffer_io(void* buf, int len, void *user)
{
   const char **ud = (const char **)user;
   if (ud[1] - ud[0] < len) len = (int)(ud[1] - ud[0]);
   memcpy(buf, ud[0], len);
   ud[0] += len;
   return len;
}

rjson_t *rjson_open_buffer(const void *buffer, size_t len)
{
   rjson_t *json   = (rjson_t *)malloc(sizeof(rjson_t) + sizeof(const char *)*2);
   const char **ud = (const char **)(json + 1);
   if (!json)
      return NULL;
   ud[0] = (const char *)buffer;
   ud[1] = ud[0] + len;
   _rjson_setup(json, _rjson_buffer_io, (void*)ud, sizeof(json->input_buf));
   return json;
}

rjson_t *rjson_open_string(const char *string, size_t len)
{
   return rjson_open_buffer(string, len);
}

static int _rjson_stream_io(void* buf, int len, void *user)
{
   return (int)intfstream_read((intfstream_t*)user, buf, (uint64_t)len);
}

rjson_t *rjson_open_stream(struct intfstream_internal *stream)
{
   /* Allocate an input buffer based on the file size */
   int64_t size = intfstream_get_size(stream);
   int io_size  =
         (size > 1024*1024 ? 4096 :
         (size >  256*1024 ? 2048 : 1024));
   return rjson_open_user(_rjson_stream_io, stream, io_size);
}

static int _rjson_rfile_io(void* buf, int len, void *user)
{
   return (int)filestream_read((RFILE*)user, buf, (int64_t)len);
}

rjson_t *rjson_open_rfile(RFILE *rfile)
{
   /* Allocate an input buffer based on the file size */
   int64_t size = filestream_get_size(rfile);
   int io_size =
         (size > 1024*1024 ? 4096 :
         (size >  256*1024 ? 2048 : 1024));
   return rjson_open_user(_rjson_rfile_io, rfile, io_size);
}

void rjson_set_options(rjson_t *json, char rjson_option_flags)
{
   json->option_flags = rjson_option_flags;
}

void rjson_set_max_depth(rjson_t *json, unsigned int max_depth)
{
   json->stack_max = max_depth;
}

const char *rjson_get_string(rjson_t *json, size_t *len)
{
   char* str             = (json->string_pass_through
         ? json->string_pass_through : json->string);
   if (len)
      *len               = json->string_len;
   str[json->string_len] = '\0';
   return str;
}

double rjson_get_double(rjson_t *json)
{
   char* str = (json->string_pass_through ? json->string_pass_through : json->string);
   str[json->string_len] = '\0';
   if (json->decimal_sep != '.')
   {
      /* handle locale that uses a non-standard decimal separator */
      char *p;
      if (json->decimal_sep == 0)
      {
         char test[4];
         snprintf(test, sizeof(test), "%.1f", 0.0f);
         json->decimal_sep = test[1];
      }
      if (json->decimal_sep != '.' && (p = strchr(str, '.')) != NULL)
      {
         double res;
         *p  = json->decimal_sep;
         res = atof(str);
         *p  = '.';
         return res;
      }
   }
   return atof(str);
}

int rjson_get_int(rjson_t *json)
{
   char* str = (json->string_pass_through ? json->string_pass_through : json->string);
   str[json->string_len] = '\0';
   return atoi(str);
}

const char *rjson_get_error(rjson_t *json)
{
   return (json->stack_top->type == RJSON_ERROR ? json->error_text : "");
}

void rjson_set_error(rjson_t *json, const char* error)
{
   _rjson_error(json, "%s", error);
}

size_t rjson_get_source_line(rjson_t *json)
{
   return json->source_line;
}

size_t rjson_get_source_column(rjson_t *json)
{
   return (json->input_p == json->source_column_p ? 1 :
         json->input_p - json->source_column_p);
}

int rjson_get_source_context_len(rjson_t *json)
{
   const unsigned char *from = json->input_buf, *to = json->input_end, *p = json->input_p;
   return (int)(((p + 256 < to ? p + 256 : to) - (p > from + 256 ? p - 256 : from)));
}

const char* rjson_get_source_context_buf(rjson_t *json)
{
   /* inside the input buffer, some " may have been replaced with \0. */
   const unsigned char *p = json->input_p, *from = json->input_buf;
   unsigned char *i = json->input_buf;
   for (; i != json->input_end; i++)
   {
      if (*i == '\0')
         *i = '"';
   }
   return (const char*)(p > from + 256 ? p - 256 : from);
}

bool rjson_check_context(rjson_t *json, unsigned int depth, ...)
{
   va_list ap;
   const struct _rjson_stack *stack = json->stack, *stack_top = json->stack_top;
   if ((unsigned int)(stack_top - stack) != depth)
      return false;
   va_start(ap, depth);
   while (++stack <= stack_top)
   {
      if (va_arg(ap, int) == (int)stack->type) continue;
      va_end(ap);
      return false;
   }
   va_end(ap);
   return true;
}

unsigned int rjson_get_context_depth(rjson_t *json)
{
   return (unsigned int)(json->stack_top - json->stack);
}

size_t rjson_get_context_count(rjson_t *json)
{
   return json->stack_top->count;
}

enum rjson_type rjson_get_context_type(rjson_t *json)
{
   return json->stack_top->type;
}

void rjson_free(rjson_t *json)
{
   if (json->stack != json->inline_stack)
      free(json->stack);
   if (json->string != json->inline_string)
      free(json->string);
   free(json);
}

static bool _rjson_nop_default(void *context) { return true; }
static bool _rjson_nop_string(void *context, const char *value, size_t len) { return true; }
static bool _rjson_nop_bool(void *context, bool value) { return true; }

enum rjson_type rjson_parse(rjson_t *json, void* context,
      bool (*object_member_handler)(void *context, const char *str, size_t len),
      bool (*string_handler       )(void *context, const char *str, size_t len),
      bool (*number_handler       )(void *context, const char *str, size_t len),
      bool (*start_object_handler )(void *context),
      bool (*end_object_handler   )(void *context),
      bool (*start_array_handler  )(void *context),
      bool (*end_array_handler    )(void *context),
      bool (*boolean_handler      )(void *context, bool value),
      bool (*null_handler         )(void *context))
{
   bool in_object = false;
   size_t _len;
   const char* string;
   if (!object_member_handler) object_member_handler = _rjson_nop_string;
   if (!string_handler       ) string_handler        = _rjson_nop_string;
   if (!number_handler       ) number_handler        = _rjson_nop_string;
   if (!start_object_handler ) start_object_handler  = _rjson_nop_default;
   if (!end_object_handler   ) end_object_handler    = _rjson_nop_default;
   if (!start_array_handler  ) start_array_handler   = _rjson_nop_default;
   if (!end_array_handler    ) end_array_handler     = _rjson_nop_default;
   if (!boolean_handler      ) boolean_handler       = _rjson_nop_bool;
   if (!null_handler         ) null_handler          = _rjson_nop_default;
   for (;;)
   {
      switch (rjson_next(json))
      {
         case RJSON_STRING:
            string = rjson_get_string(json, &_len);
            if (_rJSON_LIKELY(
                  (in_object && (json->stack_top->count & 1) ?
                     object_member_handler : string_handler)
                     (context, string, _len)))
               continue;
            return RJSON_STRING;
         case RJSON_NUMBER:
            string = rjson_get_string(json, &_len);
            if (_rJSON_LIKELY(number_handler(context, string, _len)))
               continue;
            return RJSON_NUMBER;
         case RJSON_OBJECT:
            in_object = true;
            if (_rJSON_LIKELY(start_object_handler(context)))
               continue;
            return RJSON_OBJECT;
         case RJSON_ARRAY:
            in_object = false;
            if (_rJSON_LIKELY(start_array_handler(context)))
               continue;
            return RJSON_ARRAY;
         case RJSON_OBJECT_END:
            if (_rJSON_LIKELY(end_object_handler(context)))
            {
               in_object = (json->stack_top->type == RJSON_OBJECT);
               continue;
            }
            return RJSON_OBJECT_END;
         case RJSON_ARRAY_END:
            if (_rJSON_LIKELY(end_array_handler(context)))
            {
               in_object = (json->stack_top->type == RJSON_OBJECT);
               continue;
            }
            return RJSON_ARRAY_END;
         case RJSON_TRUE:
            if (_rJSON_LIKELY(boolean_handler(context, true)))
               continue;
            return RJSON_TRUE;
         case RJSON_FALSE:
            if (_rJSON_LIKELY(boolean_handler(context, false)))
               continue;
            return RJSON_FALSE;
         case RJSON_NULL:
            if (_rJSON_LIKELY(null_handler(context)))
               continue;
            return RJSON_NULL;
         case RJSON_ERROR:
            return RJSON_ERROR;
         case RJSON_DONE:
            return RJSON_DONE;
      }
   }
}

bool rjson_parse_quick(const char *string, size_t len, void* context, char option_flags,
      bool (*object_member_handler)(void *context, const char *str, size_t len),
      bool (*string_handler       )(void *context, const char *str, size_t len),
      bool (*number_handler       )(void *context, const char *str, size_t len),
      bool (*start_object_handler )(void *context),
      bool (*end_object_handler   )(void *context),
      bool (*start_array_handler  )(void *context),
      bool (*end_array_handler    )(void *context),
      bool (*boolean_handler      )(void *context, bool value),
      bool (*null_handler         )(void *context),
      void (*error_handler        )(void *context, int line, int col, const char* error))
{
   const char *user_data[2];
   rjson_t json;
   user_data[0] = string;
   user_data[1] = string + len;
   _rjson_setup(&json, _rjson_buffer_io, (void*)user_data, sizeof(json.input_buf));
   rjson_set_options(&json, option_flags);
   if (rjson_parse(&json, context,
         object_member_handler, string_handler, number_handler,
         start_object_handler, end_object_handler,
         start_array_handler, end_array_handler,
         boolean_handler, null_handler) == RJSON_DONE)
      return true;
   if (error_handler)
      error_handler(context,
            (int)rjson_get_source_line(&json),
            (int)rjson_get_source_column(&json),
            rjson_get_error(&json));
   return false;
}

struct rjsonwriter
{
   char* buf;
   int buf_num, buf_cap;

   rjsonwriter_io_t io;
   void *user_data;

   const char* error_text;
   char option_flags, decimal_sep;
   bool buf_is_output, final_flush;

   char inline_buf[1024];
};

rjsonwriter_t *rjsonwriter_open_user(rjsonwriter_io_t io, void *user_data)
{
   rjsonwriter_t* writer = (rjsonwriter_t*)malloc(sizeof(rjsonwriter_t));
   if (!writer)
      return NULL;

   writer->buf           = writer->inline_buf;
   writer->buf_num       = 0;
   writer->buf_cap       = sizeof(writer->inline_buf);

   writer->error_text    = NULL;
   writer->option_flags  = writer->decimal_sep = 0;
   writer->buf_is_output = writer->final_flush = false;

   writer->io            = io;
   writer->user_data     = user_data;

   return writer;
}

static int _rjsonwriter_stream_io(const void* buf, int len, void *user)
{
   return (int)intfstream_write((intfstream_t*)user, buf, (uint64_t)len);
}

rjsonwriter_t *rjsonwriter_open_stream(struct intfstream_internal *stream)
{
   return rjsonwriter_open_user(_rjsonwriter_stream_io, stream);
}

static int _rjsonwriter_rfile_io(const void* buf, int len, void *user)
{
   return (int)filestream_write((RFILE*)user, buf, (int64_t)len);
}

rjsonwriter_t *rjsonwriter_open_rfile(RFILE *rfile)
{
   return rjsonwriter_open_user(_rjsonwriter_rfile_io, rfile);
}

static int _rjsonwriter_memory_io(const void* buf, int len, void *user)
{
   rjsonwriter_t *writer = (rjsonwriter_t *)user;
   bool is_append        = (buf != writer->buf);
   int new_cap           = writer->buf_num + (is_append ? len : 0) + 512;
   if (!writer->final_flush && (is_append || new_cap > writer->buf_cap))
   {
      bool can_realloc   = (writer->buf != writer->inline_buf);
      char* new_buf      = (char*)(can_realloc ? realloc(writer->buf, new_cap) : malloc(new_cap));
      if (!new_buf)
         return 0;
      if (!can_realloc)
         memcpy(new_buf, writer->buf, writer->buf_num);
      if (is_append)
      {
         memcpy(new_buf + writer->buf_num, buf, len);
         writer->buf_num += len;
      }
      writer->buf        = new_buf;
      writer->buf_cap    = new_cap;
   }
   return len;
}

rjsonwriter_t *rjsonwriter_open_memory(void)
{
   rjsonwriter_t *writer = rjsonwriter_open_user(_rjsonwriter_memory_io, NULL);
   if (!writer)
      return NULL;
   writer->user_data     = writer;
   writer->buf_is_output = true;
   return writer;
}

char* rjsonwriter_get_memory_buffer(rjsonwriter_t *writer, int* len)
{
   if (writer->io != _rjsonwriter_memory_io || writer->error_text)
      return NULL;
   if (writer->buf_num == writer->buf_cap)
      rjsonwriter_flush(writer);
   writer->buf[writer->buf_num] = '\0';
   if (len)
      *len = writer->buf_num;
   return writer->buf;
}

int rjsonwriter_count_memory_buffer(rjsonwriter_t *writer)
{
   return writer->buf_num;
}

void rjsonwriter_erase_memory_buffer(rjsonwriter_t *writer, int keep_len)
{
   if (keep_len <= writer->buf_num)
      writer->buf_num = (keep_len < 0 ? 0 : keep_len);
}

bool rjsonwriter_free(rjsonwriter_t *writer)
{
   bool res;
   writer->final_flush = true;
   res = rjsonwriter_flush(writer);
   if (writer->buf != writer->inline_buf)
      free(writer->buf);
   free(writer);
   return res;
}

void rjsonwriter_set_options(rjsonwriter_t *writer, int rjsonwriter_option_flags)
{
   writer->option_flags = rjsonwriter_option_flags;
}

bool rjsonwriter_flush(rjsonwriter_t *writer)
{
   if (writer->buf_num && !writer->error_text && writer->io(writer->buf,
            writer->buf_num, writer->user_data) != writer->buf_num)
      writer->error_text = "output error";
   if (!writer->buf_is_output || writer->error_text)
      writer->buf_num = 0;
   return !writer->error_text;
}

const char *rjsonwriter_get_error(rjsonwriter_t *writer)
{
   return (writer->error_text ? writer->error_text : "");
}

void rjsonwriter_raw(rjsonwriter_t *writer, const char *buf, int len)
{
   if (writer->buf_num + len > writer->buf_cap)
      rjsonwriter_flush(writer);
   if (len == 1)
   {
      if (buf[0] > ' ' ||
            !(writer->option_flags & RJSONWRITER_OPTION_SKIP_WHITESPACE))
         writer->buf[writer->buf_num++] = buf[0];
   }
   else
   {
      int add = writer->buf_cap - writer->buf_num;
      if (add > len)
         add = len;
      memcpy(writer->buf + writer->buf_num, buf, add);
      writer->buf_num += add;
      if (len == add)
         return;
      rjsonwriter_flush(writer);
      len -= add;
      buf += add;
      if (writer->buf_num + len <= writer->buf_cap)
      {
         memcpy(writer->buf + writer->buf_num, buf, len);
         writer->buf_num += len;
      }
      else if (writer->io(buf, len, writer->user_data) != len)
         writer->error_text = "output error";
   }
}

void rjsonwriter_rawf(rjsonwriter_t *writer, const char *fmt, ...)
{
   int available, need;
   va_list ap, ap2;
   if (writer->buf_num >= writer->buf_cap - 16)
      rjsonwriter_flush(writer);
   available = (writer->buf_cap - writer->buf_num);
   va_start(ap, fmt);
   need = vsnprintf(writer->buf + writer->buf_num, available, fmt, ap);
   va_end(ap);
   if (need <= 0)
      return;
   if (need < available)
   {
      writer->buf_num += need;
      return;
   }
   rjsonwriter_flush(writer);
   if (writer->buf_num + need >= writer->buf_cap)
   {
      int newcap   = writer->buf_num + need + 1;
      char* newbuf = (char*)malloc(newcap);
      if (!newbuf)
      {
         if (!writer->error_text)
            writer->error_text = "out of memory";
         return;
      }
      if (writer->buf_num)
         memcpy(newbuf, writer->buf, writer->buf_num);
      if (writer->buf != writer->inline_buf)
         free(writer->buf);
      writer->buf = newbuf;
      writer->buf_cap = newcap;
   }
   va_start(ap2, fmt);
   vsnprintf(writer->buf + writer->buf_num, writer->buf_cap - writer->buf_num, fmt, ap2);
   va_end(ap2);
   writer->buf_num += need;
}

void _rjsonwriter_add_escaped(rjsonwriter_t *writer, unsigned char c)
{
   char esc_buf[8], esc_len = 2;
   const char* esc;
   switch (c)
   {
      case '\b':
         esc = "\\b";
         break;
      case '\t':
         esc = "\\t";
         break;
      case '\n':
         esc = "\\n";
         break;
      case '\f':
         esc = "\\f";
         break;
      case '\r':
         esc = "\\r";
         break;
      case '\"':
         esc = "\\\"";
         break;
      case '\\':
         esc = "\\\\";
         break;
      case '/':
         esc = "\\/";
         break;
      default:
         snprintf(esc_buf, sizeof(esc_buf), "\\u%04x", c);
         esc     = esc_buf;
         esc_len = 6;
   }
   rjsonwriter_raw(writer, esc, esc_len);
}

void rjsonwriter_add_string(rjsonwriter_t *writer, const char *value)
{
   const char *p = (const char*)value, *raw = p;
   unsigned char c;
   rjsonwriter_raw(writer, "\"", 1);
   if (!p)
      goto string_end;
   while ((c = (unsigned char)*p++) != '\0')
   {
      /* forward slash is special, it should be escaped if the previous character
       * was a < (intended to avoid having </script> html tags in JSON files) */
      if (   c >= 0x20 && c != '\"' && c != '\\' &&
            (c != '/' || p < value + 2 || p[-2] != '<'))
         continue;
      if (raw != p - 1)
         rjsonwriter_raw(writer, raw, (int)(p - 1 - raw));
      _rjsonwriter_add_escaped(writer, c);
      raw = p;
   }
   if (raw != p - 1)
      rjsonwriter_raw(writer, raw, (int)(p - 1 - raw));
string_end:
   rjsonwriter_raw(writer, "\"", 1);
}

void rjsonwriter_add_string_len(rjsonwriter_t *writer, const char *value, int len)
{
   const char *p = (const char*)value, *raw = p, *end = p + len;
   rjsonwriter_raw(writer, "\"", 1);
   while (p != end)
   {
      unsigned char c = (unsigned char)*p++;
      if (      c >= 0x20 && c != '\"' && c != '\\'
            && (c != '/' || p < value + 2 || p[-2] != '<'))
         continue;
      if (raw != p - 1)
         rjsonwriter_raw(writer, raw, (int)(p - 1 - raw));
      _rjsonwriter_add_escaped(writer, c);
      raw = p;
   }
   if (raw != end)
      rjsonwriter_raw(writer, raw, (int)(end - raw));
   rjsonwriter_raw(writer, "\"", 1);
}

void rjsonwriter_add_double(rjsonwriter_t *writer, double value)
{
   int old_buf_num = writer->buf_num;
   rjsonwriter_rawf(writer, "%G", value);
   if (writer->decimal_sep != '.')
   {
      /* handle locale that uses a non-standard decimal separator */
      char *p, *str;
      if (writer->decimal_sep == 0)
      {
         char test[4];
         snprintf(test, sizeof(test), "%.1f", 0.0f);
         if ((writer->decimal_sep = test[1]) == '.')
            return;
      }
      str = writer->buf + (old_buf_num > writer->buf_num ? 0 : old_buf_num);
      if ((p = strchr(str, writer->decimal_sep)) != NULL)
         *p = '.';
   }
}

void rjsonwriter_add_spaces(rjsonwriter_t *writer, int count)
{
   if (!(writer->option_flags & RJSONWRITER_OPTION_SKIP_WHITESPACE))
      for (; count > 0; count -= 8)
         rjsonwriter_raw(writer, "        ", (count > 8 ? 8 : count));
}

void rjsonwriter_add_tabs(rjsonwriter_t *writer, int count)
{
   if (!(writer->option_flags & RJSONWRITER_OPTION_SKIP_WHITESPACE))
      for (; count > 0; count -= 8)
         rjsonwriter_raw(writer, "\t\t\t\t\t\t\t\t", (count > 8 ? 8 : count));
}

#undef _rJSON_EOF
#undef _rJSON_LIKELY

/* Copyright  (C) 2010-2021 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rjson_parser.h).
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

/* Shared parser implementation. Included for buffered and borrowed input. */

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

#ifdef RJSON_BORROWED
static bool _rjson_io_input(rjson_t *json)
{
   (void)json;
   return false;
}
#else
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

#endif

static bool _rjson_grow_string(rjson_t *json)
{
   char *string;
   size_t new_string_cap;
   /* Bound the doubling so string_cap * 2 can never wrap past
    * SIZE_MAX.  Pre-patch a pathological input on 32-bit (string_cap
    * nearing 2 GiB) doubled to 0 and realloc() returned either NULL
    * (benign) or a 0-byte pointer (heap overflow on next pushchar).
    * Post-patch we fail cleanly before the arithmetic wrap. */
   if (json->string_cap > _RJSON_MAX_SIZE / 2)
   {
      if (json->string_cap >= _RJSON_MAX_SIZE)
      {
         _rjson_error(json, "string token too large");
         return false;
      }
      new_string_cap = _RJSON_MAX_SIZE;
   }
   else
      new_string_cap = json->string_cap * 2;
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
#ifdef RJSON_BORROWED
   bool span_only = json->input_len < 0;
#endif
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
#ifdef RJSON_BORROWED
            if (span_only)
            {
               json->string_len = p - raw;
               json->string_pass_through = (char*)raw;
               if ((utf8mask & 0x80) && !_rjson_validate_utf8(json))
                  return RJSON_ERROR;
               return RJSON_STRING;
            }
#endif
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
#ifdef RJSON_BORROWED
            if (span_only)
            {
               json->string_len = p - raw;
               json->string_pass_through = (char*)raw;
               if ((utf8mask & 0x80) && !_rjson_validate_utf8(json))
                  return RJSON_ERROR;
               json->string_len = 0;
               json->string_pass_through = NULL;
               utf8mask = 0;
            }
            else
#endif
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
#ifdef RJSON_BORROWED
         if (span_only)
            return _rjson_error(json, "unterminated string literal");
#endif
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
#ifndef RJSON_BORROWED
         if (!_rjson_pushchars(json, start, p))
            return RJSON_ERROR; /* out of memory */
#endif
         break;
      }
      else
      {
#ifndef RJSON_BORROWED
         /* number sequences are always copied to the string buffer */
         if (!_rjson_pushchars(json, start, p))
            return RJSON_ERROR;
#endif
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

#ifdef RJSON_BORROWED
   p = start;
   end = json->input_p;
#else
   p = (const unsigned char *)json->string;
   end = (p + json->string_len);
#endif

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

#ifdef RJSON_BORROWED
static
#endif
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

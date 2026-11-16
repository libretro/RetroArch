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

/* rjson -- streaming JSON parser and writer.
 *
 * What it implements: a pull (SAX-style) parser over strings, buffers,
 * or a user I/O callback, delivering the
 * element stream through rjson_next with string/double/int accessors
 * and the callback-driven rjson_parse convenience driver; UTF-8
 * validation with
 * configurable handling of invalid input; opt-in extensions
 * (JavaScript comments, UTF-8 BOM, unescaped control characters,
 * trailing data - see enum rjson_option); a depth limit; and a
 * matching writer (rjsonwriter_*) with the same sink choices and
 * proper string escaping.
 *
 * What it does not implement: an in-memory DOM (callers consume the
 * event stream), JSON5/NaN/Infinity extensions beyond the listed
 * options, and sorting or pretty-printing beyond the writer's simple
 * indentation helpers.
 */

/* The parser is based on Public Domain JSON Parser for C by Christopher Wellons - https://github.com/skeeto/pdjson */

#include <stdio.h>  /* snprintf, vsnprintf */
#include <stdarg.h> /* va_list */
#include <string.h> /* memcpy */
#include <stdint.h> /* int64_t, SIZE_MAX */
#include <stdlib.h> /* malloc, realloc, atof, atoi */
#include <limits.h> /* INT_MAX */

/* Ceiling for the parser's growing string buffer and the writer's
 * in-memory buffer.  Reached in practice only by pathologically large
 * (and almost certainly malformed) JSON inputs on a 32-bit system,
 * but having an explicit cap avoids undefined behaviour in the
 * signed-int arithmetic of the writer and the size_t doubling in the
 * parser wrapping past SIZE_MAX / 2.  Callers with legitimate giant
 * payloads should not be using these APIs in the first place --
 * stream in chunks. */
#define _RJSON_MAX_SIZE ((size_t)256 * 1024 * 1024)

#include <retro_inline.h> /* INLINE - was reached transitively
                            * through the stream headers before the
                            * I/O adapters moved out */
#include <formats/rjson.h>
#include <string/rstrtod.h>
#include <compat/posix_string.h>

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

/* Compile the same grammar without mode checks in the buffered path. */
#include "rjson_parser.h"

#define RJSON_BORROWED
#define _rjson_error _rjson_error_borrowed
#define _rjson_error_char _rjson_error_char_borrowed
#define _rjson_error_token _rjson_error_token_borrowed
#define _rjson_io_input _rjson_io_input_borrowed
#define _rjson_grow_string _rjson_grow_string_borrowed
#define _rjson_pushchar _rjson_pushchar_borrowed
#define _rjson_pushchars _rjson_pushchars_borrowed
#define _rjson_char_get _rjson_char_get_borrowed
#define _rjson_get_unicode_cp _rjson_get_unicode_cp_borrowed
#define _rjson_read_unicode _rjson_read_unicode_borrowed
#define _rjson_validate_utf8 _rjson_validate_utf8_borrowed
#define _rjson_read_string _rjson_read_string_borrowed
#define _rjson_read_number _rjson_read_number_borrowed
#define _rjson_push_stack _rjson_push_stack_borrowed
#define _rjson_read_name _rjson_read_name_borrowed
#define _rjson_optional_skip _rjson_optional_skip_borrowed
#define rjson_next _rjson_next_borrowed
#include "rjson_parser.h"
#undef _rjson_error
#undef _rjson_error_char
#undef _rjson_error_token
#undef _rjson_io_input
#undef _rjson_grow_string
#undef _rjson_pushchar
#undef _rjson_pushchars
#undef _rjson_char_get
#undef _rjson_get_unicode_cp
#undef _rjson_read_unicode
#undef _rjson_validate_utf8
#undef _rjson_read_string
#undef _rjson_read_number
#undef _rjson_push_stack
#undef _rjson_read_name
#undef _rjson_optional_skip
#undef rjson_next
#undef RJSON_BORROWED

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
}

rjson_t *rjson_open_user(rjson_io_t io, void *user_data, int io_block_size)
{
   rjson_t* json;
   /* Clamp io_block_size against negative / tiny / oversized values:
    * this function is public and can be reached with anything. */
   if (io_block_size < 16)
      io_block_size = 16;
   else if ((size_t)io_block_size > _RJSON_MAX_SIZE)
      io_block_size = (int)_RJSON_MAX_SIZE;
   json = (rjson_t*)malloc(
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
   /* rstrtod reads the '.' the JSON grammar requires in any locale, so
    * the old sniff-the-separator-and-rewrite-the-buffer dance is gone. */
   return rstrtod(str, NULL);
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

/* Release the two buffers that can outgrow their inline storage.
 * Split out of rjson_free() because a stack-allocated rjson_t has
 * the same buffers to release but must not have free() called on
 * the handle itself. */
static void _rjson_free_buffers(rjson_t *json)
{
   if (json->stack != json->inline_stack)
   {
      free(json->stack);
      json->stack = json->inline_stack;
   }
   if (json->string != json->inline_string)
   {
      free(json->string);
      json->string = json->inline_string;
   }
}

void rjson_free(rjson_t *json)
{
   _rjson_free_buffers(json);
   free(json);
}


static void _rjson_setup_borrowed(rjson_t *json, const char *data, size_t length)
{
   _rjson_setup(json, NULL, NULL, 0);
   json->input_p = (const unsigned char *)data;
   json->input_end = json->input_p + length;
   json->source_column_p = json->input_p;
}

static enum rjson_type _rjson_next_span(rjson_t *json, rjson_span_t *span)
{
   const unsigned char *start = json->input_p;
   while (start != json->input_end &&
         (*start == ' ' || *start == '\t' || *start == '\n' ||
          *start == '\r' || *start == ',' || *start == ':'))
      start++;
   span->data = (const char *)start;
   span->count = 0;
   span->type = _rjson_next_borrowed(json);
   span->length = (size_t)(json->input_p - start);
   return span->type;
}

static bool _rjson_finish_span(rjson_t *json, rjson_span_t *span)
{
   if (span->type == RJSON_OBJECT || span->type == RJSON_ARRAY)
   {
      size_t depth = (size_t)(json->stack_top - json->stack);
      while ((size_t)(json->stack_top - json->stack) >= depth)
         if (_rjson_next_borrowed(json) == RJSON_ERROR)
            return false;
      span->count = json->stack[depth].count;
      if (span->type == RJSON_OBJECT)
         span->count /= 2;
      span->length = (size_t)((const char *)json->input_p - span->data);
   }
   return span->type != RJSON_ERROR && span->type != RJSON_DONE;
}

bool rjson_read_span(const void *data, size_t length, rjson_span_t *value,
      rjson_span_member_t member, void *context)
{
   rjson_t json;
   bool ok = false;
   if (!data || !length || !value)
      return false;
   _rjson_setup_borrowed(&json, (const char *)data, length);
   json.input_len = -1; /* Validate spans without materializing token text. */
   _rjson_next_span(&json, value);
   if (value->type == RJSON_OBJECT && member)
   {
      rjson_span_t name, child;
      for (;;)
      {
         enum rjson_type type = _rjson_next_span(&json, &name);
         if (type == RJSON_OBJECT_END)
         {
            value->length = (size_t)((const char *)json.input_p - value->data);
            ok = true;
            break;
         }
         if (type != RJSON_STRING)
            break;
         _rjson_next_span(&json, &child);
         if (!_rjson_finish_span(&json, &child) || !member(context, &name, &child))
            break;
         value->count++;
      }
   }
   else
      ok = _rjson_finish_span(&json, value);
   _rjson_free_buffers(&json);
   return ok;
}

static const char *_rjson_span_space(const char *p, const char *end)
{
   while (p != end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
      p++;
   return p;
}

static const char *_rjson_span_string(const char *p, const char *end)
{
   const char *begin = p + 1;
   p = begin;
   while (p != end)
   {
      const char *escape;
      p = (const char *)memchr(p, '"', (size_t)(end - p));
      if (!p)
         return NULL;
      escape = p;
      while (escape != begin && escape[-1] == '\\')
         escape--;
      if (!((size_t)(p - escape) & 1))
         return p + 1;
      p++;
   }
   return NULL;
}

/* Only delimiters need inspecting after the grammar has accepted the input. */
static const char *_rjson_span_value(const char *p, const char *end,
      rjson_span_t *value)
{
   const char *start = p;
   size_t depth = 1;
   value->data  = p;
   value->count = 0;
   switch (*p)
   {
      case '"':
         value->type = RJSON_STRING;
         p = _rjson_span_string(p, end);
         if (!p)
            return NULL;
         break;
      case '{':
      case '[':
         value->type = *p == '{' ? RJSON_OBJECT : RJSON_ARRAY;
         p = _rjson_span_space(p + 1, end);
         if (p == end)
            return NULL;
         if (*p != '}' && *p != ']')
            value->count = 1;
         for (; p != end; p++)
         {
            switch (*p)
            {
               case '"':
                  p = _rjson_span_string(p, end);
                  if (!p)
                     return NULL;
                  p--;
                  break;
               case '{':
               case '[':
                  depth++;
                  break;
               case '}':
               case ']':
                  if (!--depth)
                  {
                     p++;
                     goto finished;
                  }
                  break;
               case ',':
                  if (depth == 1)
                     value->count++;
                  break;
            }
         }
         return NULL;
      default:
         value->type = *p == 't' ? RJSON_TRUE : *p == 'f' ? RJSON_FALSE :
               *p == 'n' ? RJSON_NULL : RJSON_NUMBER;
         while (p != end && (unsigned char)*p > ' ' &&
               *p != ',' && *p != '}' && *p != ']')
            p++;
         if (p == start)
            return NULL;
         break;
   }
finished:
   value->length = (size_t)(p - start);
   return p;
}

bool rjson_read_span_validated(const void *data, size_t length,
      rjson_span_t *value, rjson_span_member_t member, void *context)
{
   const char *p, *end;
   if (!data || !length || !value)
      return false;
   end = (const char *)data + length;
   p   = _rjson_span_space((const char *)data, end);
   if (p == end)
      return false;
   if (*p != '{' || !member)
      return _rjson_span_value(p, end, value) != NULL;

   value->data  = p;
   value->count = 0;
   value->type  = RJSON_OBJECT;
   p = _rjson_span_space(p + 1, end);
   while (p != end && *p != '}')
   {
      rjson_span_t name, child;
      if (*p != '"')
         return false;
      name.data  = p;
      name.count = 0;
      name.type  = RJSON_STRING;
      p = _rjson_span_string(p, end);
      if (!p)
         return false;
      name.length = (size_t)(p - name.data);
      p = _rjson_span_space(p, end);
      if (p == end || *p++ != ':')
         return false;
      p = _rjson_span_space(p, end);
      if (p == end)
         return false;
      p = _rjson_span_value(p, end, &child);
      if (!p || !member(context, &name, &child))
         return false;
      value->count++;
      p = _rjson_span_space(p, end);
      if (p != end && *p == ',')
         p = _rjson_span_space(p + 1, end);
      else
         break;
   }
   if (p == end || *p != '}')
      return false;
   value->length = (size_t)(p + 1 - value->data);
   return true;
}

bool rjson_decode_string(const char *data, size_t length,
      char *output, size_t capacity, size_t *written)
{
   rjson_t json;
   bool ok;
   if (!data || length < 2 || *data != '"' || data[length - 1] != '"' || !output || capacity < length - 1)
      return false;
   _rjson_setup_borrowed(&json, data, length);
   json.string = output;
   json.string_cap = capacity;
   json.input_p++; /* The opening quote was checked above. */
   ok = _rjson_read_string_borrowed(&json) == RJSON_STRING && json.input_p == json.input_end;
   if (ok)
   {
      if (json.string_pass_through)
         memcpy(output, json.string_pass_through, json.string_len);
      output[json.string_len] = '\0';
      if (written)
         *written = json.string_len;
   }
   /* The output belongs to the caller, not the parser. */
   json.string = json.inline_string;
   _rjson_free_buffers(&json);
   return ok;
}

bool rjson_string_equal(const char *data, size_t length, const char *text)
{
   rjson_t json;
   bool equal;
   if (!data || length < 2 || *data != '"' || !text)
      return false;
   _rjson_setup_borrowed(&json, data, length);
   equal = _rjson_next_borrowed(&json) == RJSON_STRING && json.input_p == json.input_end;
   if (equal)
      equal = strlen(text) == json.string_len &&
            !memcmp(json.string_pass_through ? json.string_pass_through :
                  json.string, text, json.string_len);
   _rjson_free_buffers(&json);
   return equal;
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
   {
      /* The handle is on the stack, but its string and stack buffers
       * are not: either outgrows its inline storage onto the heap - a
       * long string token, or deep nesting - and nothing released
       * them on the way out.  rjson_free() cannot be used here
       * because it frees the handle too, so the buffer release is
       * split out.
       *
       * The one caller in the tree parses netplay lobby responses,
       * so the input is a remote server's and the growth is its
       * choice, up to _RJSON_MAX_SIZE.  That made this leak per
       * refresh and sized by whatever the other end sent. */
      _rjson_free_buffers(&json);
      return true;
   }
   if (error_handler)
      error_handler(context,
            (int)rjson_get_source_line(&json),
            (int)rjson_get_source_column(&json),
            rjson_get_error(&json));
   _rjson_free_buffers(&json);
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

static int _rjsonwriter_memory_io(const void* buf, int len, void *user)
{
   rjsonwriter_t *writer = (rjsonwriter_t *)user;
   bool is_append        = (buf != writer->buf);
   size_t append_len     = (is_append ? (size_t)len : 0);
   size_t target;
   int    new_cap;
   /* Detect the int-overflow pre-patch, where buf_num + len + 512
    * wrapped past INT_MAX and new_cap went negative.  The subsequent
    * "new_cap > buf_cap" comparison then misbehaved and memcpy wrote
    * past the existing allocation on a post-overflow buffer that
    * hadn't been grown.  Do the arithmetic in size_t and cap at
    * _RJSON_MAX_SIZE (which also fits comfortably in int). */
   if (len < 0 || (size_t)writer->buf_num > _RJSON_MAX_SIZE - append_len
                                           - 512)
   {
      if (!writer->error_text)
         writer->error_text = "output buffer too large";
      return 0;
   }
   target = (size_t)writer->buf_num + append_len + 512;
   new_cap = (int)target;
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
   /* Guard against negative len and against buf_num+len signed
    * overflow.  A caller that somehow passes a huge len would
    * pre-patch compute a negative sum, skip the flush, then memcpy
    * past the existing buffer below. */
   if (len < 0)
      return;
   if ((size_t)writer->buf_num + (size_t)len > (size_t)writer->buf_cap)
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
   /* Guard signed-int overflow on newcap: buf_num + need + 1 could
    * wrap past INT_MAX for a single very large formatted value. */
   if ((size_t)writer->buf_num + (size_t)need + 1 > _RJSON_MAX_SIZE)
   {
      if (!writer->error_text)
         writer->error_text = "output buffer too large";
      return;
   }
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

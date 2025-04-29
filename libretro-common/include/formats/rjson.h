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

#ifndef __LIBRETRO_SDK_FORMAT_RJSON_H__
#define __LIBRETRO_SDK_FORMAT_RJSON_H__

#include <retro_common_api.h>
#include <boolean.h> /* bool */
#include <stddef.h> /* size_t */

RETRO_BEGIN_DECLS

/* List of possible element types returned by rjson_next */
enum rjson_type
{
   RJSON_DONE,
   RJSON_OBJECT, RJSON_ARRAY, RJSON_OBJECT_END, RJSON_ARRAY_END,
   RJSON_STRING, RJSON_NUMBER, RJSON_TRUE, RJSON_FALSE, RJSON_NULL,
   RJSON_ERROR
};

/* Options that can be passed to rjson_set_options */
enum rjson_option
{
   /* Allow UTF-8 byte order marks */
   RJSON_OPTION_ALLOW_UTF8BOM                      = (1<<0),
   /* Allow JavaScript style comments in the stream */
   RJSON_OPTION_ALLOW_COMMENTS                     = (1<<1),
   /* Allow unescaped control characters in strings (bytes 0x00 - 0x1F) */
   RJSON_OPTION_ALLOW_UNESCAPED_CONTROL_CHARACTERS = (1<<2),
   /* Ignore invalid Unicode escapes and don't validate UTF-8 codes */
   RJSON_OPTION_IGNORE_INVALID_ENCODING            = (1<<3),
   /* Replace invalid Unicode escapes and UTF-8 codes with a '?' character */
   RJSON_OPTION_REPLACE_INVALID_ENCODING           = (1<<4),
   /* Ignore carriage return (\r escape sequence) in strings */
   RJSON_OPTION_IGNORE_STRING_CARRIAGE_RETURN      = (1<<5),
   /* Allow data after the end of the top JSON object/array/value */
   RJSON_OPTION_ALLOW_TRAILING_DATA                = (1<<6)
};

/* Custom data input callback
 * Should return > 0 and <= len on success, 0 on file end and < 0 on error. */
typedef int (*rjson_io_t)(void* buf, int len, void *user_data);
typedef struct rjson rjson_t;
struct intfstream_internal;
struct RFILE;

/* Create a new parser instance from various sources */
rjson_t *rjson_open_stream(struct intfstream_internal *stream);
rjson_t *rjson_open_rfile(struct RFILE *rfile);
rjson_t *rjson_open_buffer(const void *buffer, size_t len);
rjson_t *rjson_open_string(const char *string, size_t len);
rjson_t *rjson_open_user(rjson_io_t io, void *user_data, int io_block_size);

/* Free the parser instance created with rjson_open_* */
void rjson_free(rjson_t *json);

/* Set one or more enum rjson_option, will override previously set options.
 * Use bitwise OR to concatenate multiple options.
 * By default none of the options are set. */
void rjson_set_options(rjson_t *json, char rjson_option_flags);

/* Sets the maximum context depth, recursion inside arrays and objects.
 * By default this is set to 50. */
void rjson_set_max_depth(rjson_t *json, unsigned int max_depth);

/* Parse to the next JSON element and return the type of it.
 * Will return RJSON_DONE when successfully reaching the end or
 * RJSON_ERROR when an error was encountered. */
enum rjson_type rjson_next(rjson_t *json);

/* Get the current string, null-terminated unescaped UTF-8 encoded.
 * Can only be used when the current element is RJSON_STRING or RJSON_NUMBER.
 * The returned pointer is only valid until the parsing continues. */
const char *rjson_get_string(rjson_t *json, size_t *length);

/* Returns the current number (or string) converted to double or int */
double rjson_get_double(rjson_t *json);
int    rjson_get_int(rjson_t *json);

/* Returns a string describing the error once rjson_next/rjson_parse
 * has returned an unrecoverable RJSON_ERROR (otherwise returns ""). */
const char *rjson_get_error(rjson_t *json);

/* Can be used to set a custom error description on an invalid JSON structure.
 * Maximum length of 79 characters and once set the parsing can't continue. */
void rjson_set_error(rjson_t *json, const char* error);

/* Functions to get the current position in the source stream as well as */
/* a bit of source json around the current position for additional detail
 * when parsing has failed with RJSON_ERROR.
 * Intended to be used with printf style formatting like:
 * printf("Invalid JSON at line %d, column %d - %s - Source: ...%.*s...\n",
 *       (int)rjson_get_source_line(json), (int)rjson_get_source_column(json),
 *       rjson_get_error(json), rjson_get_source_context_len(json),
 *       rjson_get_source_context_buf(json)); */
size_t      rjson_get_source_line(rjson_t *json);
size_t      rjson_get_source_column(rjson_t *json);
int         rjson_get_source_context_len(rjson_t *json);
const char* rjson_get_source_context_buf(rjson_t *json);

/* Confirm the parsing context stack, for example calling
   rjson_check_context(json, 2, RJSON_OBJECT, RJSON_ARRAY)
   returns true when inside "{ [ ..." but not for "[ .." or "{ [ { ..." */
bool rjson_check_context(rjson_t *json, unsigned int depth, ...);

/* Returns the current level of nested objects/arrays */
unsigned int rjson_get_context_depth(rjson_t *json);

/* Return the current parsing context, that is, RJSON_OBJECT if we are inside
 * an object, RJSON_ARRAY if we are inside an array, and RJSON_DONE or
 * RJSON_ERROR if we are not yet/anymore in either. */
enum rjson_type rjson_get_context_type(rjson_t *json);

/* While inside an object or an array, this return the number of parsing
 * events that have already been observed at this level with rjson_next.
 * In particular, inside an object, an odd number would indicate that the just
 * observed RJSON_STRING event is a member name. */
size_t rjson_get_context_count(rjson_t *json);

/* Parse an entire JSON stream with a list of element specific handlers.
 * Each of the handlers can be passed a function or NULL to ignore it.
 * If a handler returns false, the parsing will abort and the returned
 * rjson_type will indicate on which element type parsing was aborted.
 * Otherwise the return value will be RJSON_DONE or RJSON_ERROR. */
enum rjson_type rjson_parse(rjson_t *json, void* context,
      bool (*object_member_handler)(void *context, const char *str, size_t len),
      bool (*string_handler       )(void *context, const char *str, size_t len),
      bool (*number_handler       )(void *context, const char *str, size_t len),
      bool (*start_object_handler )(void *context),
      bool (*end_object_handler   )(void *context),
      bool (*start_array_handler  )(void *context),
      bool (*end_array_handler    )(void *context),
      bool (*boolean_handler      )(void *context, bool value),
      bool (*null_handler         )(void *context));

/* A simpler interface to parse a JSON in memory. This will avoid any memory
 * allocations unless the document contains strings longer than 512 characters.
 * In the error handler, error will be "" if any of the other handlers aborted. */
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
      void (*error_handler        )(void *context, int line, int col, const char* error));

/* ------------------------------------------------------------------------- */

/* Options that can be passed to rjsonwriter_set_options */
enum rjsonwriter_option
{
   /* Don't write spaces, tabs or newlines to the output (except in strings) */
   RJSONWRITER_OPTION_SKIP_WHITESPACE = (1<<0)
};

/* Custom data output callback
 * Should return len on success and < len on a write error. */
typedef int (*rjsonwriter_io_t)(const void* buf, int len, void *user_data);
typedef struct rjsonwriter rjsonwriter_t;

/* Create a new writer instance to various targets */
rjsonwriter_t *rjsonwriter_open_stream(struct intfstream_internal *stream);
rjsonwriter_t *rjsonwriter_open_rfile(struct RFILE *rfile);
rjsonwriter_t *rjsonwriter_open_memory(void);
rjsonwriter_t *rjsonwriter_open_user(rjsonwriter_io_t io, void *user_data);

/* When opened with rjsonwriter_open_memory, will return the generated JSON.
 * Result is always null-terminated. Passed len can be NULL if not needed,
 * otherwise returned len will be string length without null-terminator.
 * Returns NULL if writing ran out of memory or not opened from memory.
 * Returned buffer is only valid until writer is modified or freed. */
char* rjsonwriter_get_memory_buffer(rjsonwriter_t *writer, int* len);

/* When opened with rjsonwriter_open_memory, will return current length */
int rjsonwriter_count_memory_buffer(rjsonwriter_t *writer);

/* When opened with rjsonwriter_open_memory, will clear the buffer.
   The buffer will be partially erased if keep_len is > 0.
   No memory is freed or re-allocated with this function. */
void rjsonwriter_erase_memory_buffer(rjsonwriter_t *writer, int keep_len);

/* Free rjsonwriter handle and return result of final rjsonwriter_flush call */
bool rjsonwriter_free(rjsonwriter_t *writer);

/* Set one or more enum rjsonwriter_option, will override previously set options.
 * Use bitwise OR to concatenate multiple options.
 * By default none of the options are set. */
void rjsonwriter_set_options(rjsonwriter_t *writer, int rjsonwriter_option_flags);

/* Flush any buffered output data to the output stream.
 * Returns true if the data was successfully written. Once writing fails once,
 * no more data will be written and flush will always returns false */
bool rjsonwriter_flush(rjsonwriter_t *writer);

/* Returns a string describing an error or "" if there was none.
 * The only error possible is "output error" after the io function failed.
 * If rjsonwriter_rawf were used manually, "out of memory" is also possible. */
const char *rjsonwriter_get_error(rjsonwriter_t *writer);

/* Used by the inline functions below to append raw data */
void rjsonwriter_raw(rjsonwriter_t *writer, const char *buf, int len);
void rjsonwriter_rawf(rjsonwriter_t *writer, const char *fmt, ...);

/* Add a UTF-8 encoded string
 * Special and control characters are automatically escaped.
 * If NULL is passed an empty string will be written (not JSON null). */
void rjsonwriter_add_string(rjsonwriter_t *writer, const char *value);
void rjsonwriter_add_string_len(rjsonwriter_t *writer, const char *value, int len);

void rjsonwriter_add_double(rjsonwriter_t *writer, double value);

void rjsonwriter_add_spaces(rjsonwriter_t *writer, int count);

void rjsonwriter_add_tabs(rjsonwriter_t *writer, int count);

RETRO_END_DECLS

#endif

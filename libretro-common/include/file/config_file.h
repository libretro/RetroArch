/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (config_file.h).
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

#ifndef __LIBRETRO_SDK_CONFIG_FILE_H
#define __LIBRETRO_SDK_CONFIG_FILE_H

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include <boolean.h>

#define CONFIG_GET_BOOL_BASE(conf, base, var, key) do { \
   bool tmp = false; \
   if (config_get_bool(conf, key, &tmp)) \
      base->var = tmp; \
} while(0)

#define CONFIG_GET_INT_BASE(conf, base, var, key) do { \
   int tmp = 0; \
   if (config_get_int(conf, key, &tmp)) \
      base->var = tmp; \
} while(0)

#define CONFIG_GET_FLOAT_BASE(conf, base, var, key) do { \
   float tmp = 0.0f; \
   if (config_get_float(conf, key, &tmp)) \
      base->var = tmp; \
} while(0)

enum config_file_flags
{
   CONF_FILE_FLG_MODIFIED                 = (1 << 0),
   CONF_FILE_FLG_GUARANTEED_NO_DUPLICATES = (1 << 1)
};

struct config_file
{
   char *path;
   struct config_entry_list **entries_map;
   struct config_entry_list *entries;
   struct config_entry_list *tail;
   struct config_entry_list *last;
   struct config_include_list *includes;
   struct path_linked_list *references;
   unsigned include_depth;
   uint8_t flags;
};

typedef struct config_file config_file_t;

struct config_file_cb
{
   void (*config_file_new_entry_cb)(char*, char*);
};

typedef struct config_file_cb config_file_cb_t ;

/* Config file format
 * - # are treated as comments. Rest of the line is ignored.
 * - Format is: key = value. There can be as many spaces as you like in-between.
 * - Value can be wrapped inside "" for multiword strings. (foo = "hai u")
 * - #include includes a config file in-place.
 *
 * Path is relative to where config file was loaded unless an absolute path is chosen.
 * Key/value pairs from an #include are read-only, and cannot be modified.
 */

/**
 * config_file_new:
 *
 * Loads a config file.
 * If @path is NULL, will create an empty config file.
 *
 * @return Returns NULL if file doesn't exist.
 **/
config_file_t *config_file_new(const char *path);

config_file_t *config_file_new_alloc(void);

/**
 * config_file_initialize:
 *
 * Leaf function.
 **/
void config_file_initialize(struct config_file *conf);

/**
 * config_file_new_with_callback:
 *
 * Loads a config file.
 * If @path is NULL, will create an empty config file.
 * Includes cb callbacks  to run custom code during config file processing.
 *
 * @return Returns NULL if file doesn't exist.
 **/
config_file_t *config_file_new_with_callback(
      const char *path, config_file_cb_t *cb);

/**
 * config_file_new_from_string:
 *
 * Load a config file from a string.
 *
 * NOTE: This will modify @from_string.
 * Pass a copy of source string if original
 * contents must be preserved
 **/
config_file_t *config_file_new_from_string(char *from_string,
      const char *path);

config_file_t *config_file_new_from_path_to_string(const char *path);

/**
 * config_file_free:
 *
 * Frees config file.
 **/
void config_file_free(config_file_t *conf);

size_t config_file_add_reference(config_file_t *conf, char *path);

bool config_file_deinitialize(config_file_t *conf);

/**
 * config_append_file:
 *
 * Loads a new config, and appends its data to @conf.
 * The key-value pairs of the new config file takes priority over the old.
 **/
bool config_append_file(config_file_t *conf, const char *path);

/* All extract functions return true when value is valid and exists.
 * Returns false otherwise. */

struct config_entry_list
{
   char *key;
   char *value;
   struct config_entry_list *next;
   /* If we got this from an #include,
    * do not allow overwrite. */
   bool readonly;
};

struct config_file_entry
{
   const char *key;
   const char *value;
   /* Used intentionally. Opaque here. */
   const struct config_entry_list *next;
};

struct config_entry_list *config_get_entry(
      const config_file_t *conf, const char *key);

/**
 * config_get_entry_list_head:
 *
 * Leaf function.
 **/
bool config_get_entry_list_head(config_file_t *conf,
      struct config_file_entry *entry);

/**
 * config_get_entry_list_next:
 *
 * Leaf function.
 **/
bool config_get_entry_list_next(struct config_file_entry *entry);

/**
 * config_get_double:
 *
 * Extracts a double from config file.
 *
 * Hidden non-leaf function cost:
 * - Calls config_get_entry()
 * - Calls strtod
 *
 * @return True if double found, otherwise false.
 **/
bool config_get_double(config_file_t *conf, const char *entry, double *in);

/**
 * config_get_float:
 *
 * Extracts a float from config file.
 *
 * Hidden non-leaf function cost:
 * - Calls config_get_entry()
 * - Calls strtod
 *
 * @return true if found, otherwise false.
 **/
bool config_get_float(config_file_t *conf, const char *entry, float *in);

/* Extracts an int from config file. */
bool config_get_int(config_file_t *conf, const char *entry, int *in);

/* Extracts an uint from config file. */
bool config_get_uint(config_file_t *conf, const char *entry, unsigned *in);

/* Extracts an size_t from config file. */
bool config_get_size_t(config_file_t *conf, const char *key, size_t *in);

#if defined(__STDC_VERSION__) && __STDC_VERSION__>=199901L
/* Extracts an uint64 from config file. */
bool config_get_uint64(config_file_t *conf, const char *entry, uint64_t *in);
#endif

/* Extracts an unsigned int from config file treating input as hex. */
bool config_get_hex(config_file_t *conf, const char *entry, unsigned *in);

/**
 * config_get_char:
 *
 * Extracts a single char from config file.
 * If value consists of several chars, this is an error.
 *
 * Hidden non-leaf function cost:
 * - Calls config_get_entry()
 *
 * @return true if found, otherwise false.
 **/
bool config_get_char(config_file_t *conf, const char *entry, char *in);

/**
 * config_get_string:
 *
 * Extracts an allocated string in *in. This must be free()-d if
 * this function succeeds.
 *
 * Hidden non-leaf function cost:
 * - Calls config_get_entry()
 * - Calls strdup
 *
 * @return true if found, otherwise false.
 **/
bool config_get_string(config_file_t *conf, const char *entry, char **in);

/* Extracts a string to a preallocated buffer. Avoid memory allocation. */
bool config_get_array(config_file_t *conf, const char *entry, char *s, size_t len);

/**
  * config_get_config_path:
  *
  * Extracts a string to a preallocated buffer.
  * Avoid memory allocation.
  *
  * Hidden non-leaf function cost:
  * - Calls strlcpy
  **/
bool config_get_config_path(config_file_t *conf, char *s, size_t len);

/* Extracts a string to a preallocated buffer. Avoid memory allocation.
 * Recognized magic like ~/. Similar to config_get_array() otherwise. */
bool config_get_path(config_file_t *conf, const char *entry, char *s, size_t len);

/**
 * config_get_bool:
 *
 * Extracts a boolean from config.
 * Valid boolean true are "true" and "1". Valid false are "false" and "0".
 * Other values will be treated as an error.
 *
 * Hidden non-leaf function cost:
 * - Calls string_is_equal() x times
 *
 * @return true if preconditions are true, otherwise false.
 **/
bool config_get_bool(config_file_t *conf, const char *entry, bool *in);

/* Setters. Similar to the getters.
 * Will not write to entry if the entry was obtained from an #include. */
size_t config_set_double(config_file_t *conf, const char *entry, double value);
size_t config_set_float(config_file_t *conf, const char *entry, float value);
size_t config_set_int(config_file_t *conf, const char *entry, int val);
size_t config_set_hex(config_file_t *conf, const char *entry, unsigned val);
size_t config_set_uint64(config_file_t *conf, const char *entry, uint64_t val);
size_t config_set_char(config_file_t *conf, const char *entry, char val);
size_t config_set_uint(config_file_t *conf, const char *key, unsigned int val);

void config_set_path(config_file_t *conf, const char *entry, const char *val);
void config_set_string(config_file_t *conf, const char *entry, const char *val);
void config_unset(config_file_t *conf, const char *key);

/**
 * config_file_write:
 *
 * Write the current config to a file.
 **/
bool config_file_write(config_file_t *conf, const char *path, bool val);

/**
 * config_file_dump:
 *
 * Dump the current config to an already opened file.
 * Does not close the file.
 **/
void config_file_dump(config_file_t *conf, FILE *file, bool val);

RETRO_END_DECLS

#endif

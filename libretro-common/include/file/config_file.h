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

/* Per-entry ownership bits (struct config_entry_list::flags).
 * Entries parsed from a buffer the conf owns borrow their key and
 * value strings from it - the strings live exactly as long as the
 * conf, no per-entry allocation or copy is made, and the free paths
 * skip them.  Entries created or overwritten through config_set_*
 * own heap strings as before.  Treat key/value of entries with a
 * BORROWED bit as read-only storage. */
#define CONF_ENTRY_FLG_KEY_BORROWED (1 << 0)
#define CONF_ENTRY_FLG_VAL_BORROWED (1 << 1)
/* Entry struct lives in a conf-owned pool block, not an individual
 * malloc: teardown releases the block, never the entry. */
#define CONF_ENTRY_FLG_POOLED       (1 << 2)

/* A text buffer owned by a config_file that entries borrow from.
 * 'io' is the interface the buffer came from (NULL: plain free). */
struct config_file_owned_buf
{
   struct config_file_owned_buf *next;
   char *data;
   const struct config_file_io *io;
};


struct config_file_entry_pool;

struct config_file
{
   char *path;
   struct config_entry_list **entries_map;
   /* Chain of adopted text buffers entries borrow from (see
    * CONF_ENTRY_FLG_*); released at deinitialize, spliced to the
    * parent on include/append pilfering. */
   struct config_file_owned_buf *owned_bufs;
   /* Interface this config reaches files through, including the files
    * named by its '#include' directives.  NULL means the process-wide
    * default; set per config by the *_with_io constructors. */
   const struct config_file_io *io;
   /* Chain of entry-pool blocks (see CONF_ENTRY_FLG_POOLED);
    * lifetime and pilfer semantics match owned_bufs. */
   struct config_file_entry_pool *entry_pool;
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

/**
 * config_file_new_take_string:
 *
 * Like config_file_new_from_string, but takes ownership of
 * @from_string instead of copying out of it: the conf adopts the
 * buffer and entries borrow their key/value strings straight from
 * it, the same zero-copy parse the path loaders use.  The buffer
 * must come from the malloc family (it is released with free() at
 * conf teardown) and must not be touched or freed by the caller
 * afterwards - ownership transfers unconditionally, including on
 * failure.  @s_len is the string length if the caller knows it, 0
 * otherwise (sizing hint only).
 **/
config_file_t *config_file_new_take_string(char *from_string,
      size_t s_len, const char *path);

/**
 * config_file_new_with_io:
 *
 * Loads the config at @path through @io instead of the process-wide
 * default, and resolves its '#include' directives through @io as well.
 *
 * The interface is recorded on the returned config rather than in a
 * global, so a caller serving configs from somewhere other than the
 * file system - members of an archive, a download held in memory, a
 * virtual file system - can do so without disturbing config loads on
 * other threads.  @io must outlive the returned config.
 *
 * @return NULL if the config could not be read or parsed.
 **/
config_file_t *config_file_new_with_io(const char *path,
      const struct config_file_io *io);

/**
 * config_file_new_take_string_with_io:
 *
 * config_file_new_take_string(), but '#include' directives inside
 * @from_string are resolved through @io rather than the default.
 *
 * This is the case of a config whose text is already in hand while the
 * files it includes are not reachable by path - an archive member being
 * the obvious one, where without an @io the include resolves to a path
 * beside the archive and is silently dropped.  Ownership of
 * @from_string transfers exactly as in config_file_new_take_string();
 * @io must outlive the returned config.
 **/
config_file_t *config_file_new_take_string_with_io(char *from_string,
      size_t s_len, const char *path, const struct config_file_io *io);

/* Streaming (push) parser: the codec-style ingest path.
 *
 * For consumers whose config bytes arrive in pieces - a network
 * fetch, an archive entry inflating, a data_transfer fill budgeted
 * across frames - instead of as one resident buffer.  Open a
 * stream, push packets of any size in any split (line boundaries
 * need not align with packet boundaries), then finish:
 *
 *    config_file_stream_t *st = config_file_stream_new(path);
 *    while ((n = source_read(chunk, sizeof(chunk))) > 0)
 *       config_file_stream_push(st, chunk, n);
 *    conf = config_file_stream_finish(st);
 *
 * Memory held is one window of the unconsumed tail: complete lines
 * are parsed and discarded as each push arrives, so the window's
 * size is bounded by the longest line plus the largest packet, not
 * by the file.  Parsed output is identical to handing the same
 * bytes to config_file_new_from_string in one piece.
 *
 * @path is recorded as the config's path (include resolution,
 * reference abbreviation); it may be NULL.  '#include' directives
 * resolve through the registered io interface as usual.
 *
 * push returns false only on allocation failure; the stream then
 * swallows further pushes and finish returns NULL.  finish frees
 * the stream and returns the parsed config (caller owns it).
 * config_file_stream_free abandons a stream part-way, freeing
 * everything including the partial config. */
typedef struct config_file_stream config_file_stream_t;

config_file_stream_t *config_file_stream_new(const char *path);

bool config_file_stream_push(config_file_stream_t *stream,
      const void *data, size_t len);

config_file_t *config_file_stream_finish(config_file_stream_t *stream);

void config_file_stream_free(config_file_stream_t *stream);

config_file_t *config_file_new_from_path_to_string(const char *path);

/* File access used by the parser core.
 *
 * config_file.c itself performs no file I/O: every byte it parses is
 * either handed to it directly (config_file_new_from_string) or
 * fetched through this interface - the path-based constructors and
 * the '#include' directive both route through it.  The interface is
 * how a host decides where config bytes come from: the plain-VFS
 * implementation in config_file_io.c, an archive, a network fetch,
 * or a test harness.
 *
 * read_file returns the entire file as a heap buffer with a NUL
 * appended after *len content bytes (the parser consumes the buffer
 * in place and relies on the terminator), or NULL on any failure.
 * free_file releases a buffer read_file returned.
 *
 * The default is registered once at startup, before any threads
 * that touch config files exist, and is only read afterwards.
 * The path-based constructors in config_file_io.c self-register the
 * filestream implementation, so any binary that links that file and
 * loads at least one config by path is covered; a host whose first
 * config comes from a string and may contain '#include' directives
 * must register explicitly first.  With no interface registered,
 * path loads fail as file-not-found and include directives are
 * recorded in the include list but not loaded. */
struct config_file_io
{
   char *(*read_file)(const char *path, int64_t *len, void *ud);
   void  (*free_file)(char *buf, void *ud);
   void   *ud;
};

typedef struct config_file_io config_file_io_t;

void config_file_set_io_default(const config_file_io_t *io);

const config_file_io_t *config_file_get_io_default(void);

/* The filestream/VFS-backed implementation (config_file_io.c). */
const config_file_io_t *config_file_io_filestream(void);

/**
 * config_file_load_file:
 *
 * Loads the file at @path (through the registered io interface)
 * into an already-initialized @conf.  Building block for the
 * path-based constructors in config_file_io.c.
 *
 * Returns 0 on success, 1 if the file could not be read (@conf is
 * untouched), -1 on allocation failure (@conf must be freed).
 **/
int config_file_load_file(config_file_t *conf, const char *path,
      config_file_cb_t *cb);

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

/**
 * config_file_append_conf:
 *
 * Appends the entries of @new_conf to @conf - the merge half of
 * config_append_file, usable directly when the second config was
 * obtained some other way (parsed from a string, streamed in).
 * The key-value pairs of @new_conf take priority over @conf's.
 * Consumes @new_conf: its entries are pilfered and the struct is
 * freed regardless of outcome.
 **/
bool config_file_append_conf(config_file_t *conf, config_file_t *new_conf);

/* All extract functions return true when value is valid and exists.
 * Returns false otherwise. */

struct config_entry_list
{
   char *key;
   char *value;
   struct config_entry_list *next;
   /* Cached strlen of key and value, so the write path does not
    * re-measure every string it just assembled.  Zero means
    * "not known" - readers must fall back to strlen - which covers
    * entries whose strings were replaced without updating these,
    * and the pathological case of a string longer than 65535 bytes.
    * Both fit in what was structure padding, so caching them costs
    * no memory per entry. */
   uint16_t key_len;
   uint16_t value_len;
   /* If we got this from an #include,
    * do not allow overwrite. */
   bool readonly;
   /* CONF_ENTRY_FLG_* ownership bits */
   uint8_t flags;
};

/* A block of entry structs owned by a config_file (see
 * CONF_ENTRY_FLG_POOLED).  Entries are bump-allocated from 'slab';
 * the block is released whole at deinitialize and spliced to the
 * parent on include/append pilfering, exactly like owned_bufs. */
struct config_file_entry_pool
{
   struct config_file_entry_pool *next;
   size_t used;
   size_t cap;
   struct config_entry_list slab[1]; /* over-allocated to cap */
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

/**
 * config_take_string:
 *
 * Removes @key's value from the config and returns it as a heap
 * string the caller owns (release with free()).  Returns NULL if the
 * key is absent or its value empty, leaving the entry untouched.
 *
 * This is the safe form of the historical idiom that lifted
 * entry->value out of the entry and NULLed it: entries parsed from a
 * path or taken string BORROW their strings from a buffer the conf
 * owns (see CONF_ENTRY_FLG_*), so that idiom now hands out a pointer
 * into the middle of the conf's buffer - reading it after
 * config_file_free is use-after-free, and free()ing it corrupts the
 * heap.  This function copies when the value is borrowed and steals
 * it when the entry owns it, so the caller holds a real allocation
 * either way.
 **/
char *config_take_string(config_file_t *conf, const char *key);

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
size_t config_get_config_path(config_file_t *conf, char *s, size_t len);

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

/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (config_file.c).
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

#include <retro_miscellaneous.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <compat/msvc.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <array/rhmap.h>

#define MAX_INCLUDE_DEPTH 16

/* All file access goes through this interface - the parser core
 * performs no file I/O of its own (no streams/file_stream.h, no
 * fopen): bytes are handed in, or fetched through the registered
 * implementation, the way the codecs under formats/ consume
 * caller-supplied data.  See the contract in config_file.h.
 *
 * Written once at startup before config-touching threads exist,
 * read-only afterwards. */
static const config_file_io_t *config_file_io_default = NULL;

void config_file_set_io_default(const config_file_io_t *io)
{
   config_file_io_default = io;
}

const config_file_io_t *config_file_get_io_default(void)
{
   return config_file_io_default;
}

struct config_include_list
{
   char *path;
   struct config_include_list *next;
};

/* C-locale isgraph(): true for 0x21..0x7e, i.e. printable ASCII
 * that is not a space.  The ctype.h isgraph() this replaces is
 * locale-sensitive: a frontend that calls setlocale() (menu
 * localization does) changes which bytes the parser accepts as
 * key/value characters, so the same config could parse differently
 * depending on when it is loaded.  The explicit range pins the
 * grammar to what the C locale always accepted and drops the
 * per-byte __ctype_b table load from the two hottest scan loops. */
#define CONFIG_FILE_ISGRAPH(c) ((unsigned char)((unsigned char)(c) - 0x21) < 0x5e)

/* Internal key hash for the entries map (murmur3-32 over 4-byte
 * words).  rhmap's default rhmap_hash_string is FNV-1a, whose
 * one-multiply-per-byte dependency chain was the largest single
 * cost left in the parse profile after the fused scan; mixing four
 * bytes per step cuts that chain ~4x on typical 10-30 byte keys.
 * The hash is purely module-internal: every insert, lookup and
 * delete on entries_map goes through the _FULL rhmap macros with
 * this value (the three direct RHMAP_*_STR pokes configuration.c
 * used to make were converted to config_get_entry), it is never
 * serialized, and stored hashes travel verbatim through the
 * pilfer/merge paths, so insert- and lookup-side agreement is the
 * only requirement - pinned by the hash-agreement test in
 * samples/file/config_file and by the differential fuzzer.
 * Word loads go through memcpy (alignment-safe) and stay strictly
 * inside [s, s+len), so no over-read at buffer ends; the result is
 * endian-dependent, which is fine for a per-process table. */
static uint32_t config_hash_span(const char *s, size_t _len)
{
   uint32_t k;
   uint32_t h = 0x811c9dc5u ^ (uint32_t)_len;
   while (_len >= 4)
   {
      memcpy(&k, s, 4);
      k    *= 0xcc9e2d51u;
      k     = (k << 15) | (k >> 17);
      k    *= 0x1b873593u;
      h    ^= k;
      h     = (h << 13) | (h >> 19);
      h     = h * 5 + 0xe6546b64u;
      s    += 4;
      _len -= 4;
   }
   if (_len)
   {
      k = 0;
      memcpy(&k, s, _len);
      k *= 0xcc9e2d51u;
      k  = (k << 15) | (k >> 17);
      k *= 0x1b873593u;
      h ^= k;
   }
   h ^= h >> 16;
   h *= 0x85ebca6bu;
   h ^= h >> 13;
   h *= 0xc2b2ae35u;
   h ^= h >> 16;
   return (h ? h : 1);
}

/* Internal parse option: entries borrow key/value strings from the
 * buffer being parsed instead of copying them out.  Only legal when
 * the conf owns that buffer for its whole lifetime (adopted via
 * config_file_adopt_buffer) - the load paths qualify, the public
 * from_string (caller-owned buffer) and the streaming window
 * (slides) do not. */
#define CONFIG_FILE_PARSE_BORROW (1 << 0)

/* Shared storage for borrowed empty values: flagged VAL_BORROWED so
 * it is never freed, and never written through (see the read-only
 * contract on CONF_ENTRY_FLG_* in config_file.h). */
static char config_file_empty_value[1] = "";

/* Cache a strlen into a uint16_t length field, storing 0 ("not
 * known", readers fall back to strlen) for anything that would not
 * fit.  Config strings are keys and values, not documents, so the
 * clamp is unreachable in practice - but a shader preset or a very
 * long path must not be silently mis-measured. */
static uint16_t config_file_cache_len(size_t _len)
{
   return (_len > 0xffff) ? 0 : (uint16_t)_len;
}

/* Take ownership of a text buffer entries will borrow from.
 * Returns false on allocation failure - the caller then parses
 * without BORROW and frees the buffer itself as before. */
static bool config_file_adopt_buffer(config_file_t *conf,
      char *buf, const config_file_io_t *io)
{
   struct config_file_owned_buf *node = (struct config_file_owned_buf*)
         malloc(sizeof(*node));
   if (!node)
      return false;
   node->data      = buf;
   node->io        = io;
   node->next      = conf->owned_bufs;
   conf->owned_bufs = node;
   return true;
}

/* Bump-allocate an entry struct from the conf's pool, growing the
 * block chain as needed.  'hint' sizes the first block (an entry
 * count estimate from the buffer length); later blocks double up to
 * a cap.  Falls back to NULL on OOM - the caller then takes the
 * plain-malloc path, so allocation failure only loses the pooling,
 * not the entry. */
static struct config_entry_list *config_file_entry_pool_alloc(
      config_file_t *conf, size_t hint)
{
   struct config_file_entry_pool *blk = conf->entry_pool;
   if (!blk || blk->used == blk->cap)
   {
      size_t cap = hint;
      if (blk)
      {
         cap = blk->cap * 2;
         if (cap > 1024)
            cap = 1024;
      }
      if (cap < 16)
         cap = 16;
      blk = (struct config_file_entry_pool*)malloc(
            sizeof(*blk) + (cap - 1) * sizeof(struct config_entry_list));
      if (!blk)
         return NULL;
      blk->used        = 0;
      blk->cap         = cap;
      blk->next        = conf->entry_pool;
      conf->entry_pool = blk;
   }
   return &blk->slab[blk->used++];
}

/* Give back the most recent pool allocation (parse rejected the
 * line).  Only ever called for the entry handed out last, so a
 * bump-down is exact. */
static void config_file_entry_pool_unwind(config_file_t *conf)
{
   if (conf->entry_pool && conf->entry_pool->used)
      conf->entry_pool->used--;
}

static void config_file_free_pool(config_file_t *conf)
{
   struct config_file_entry_pool *blk = conf->entry_pool;
   while (blk)
   {
      struct config_file_entry_pool *hold = blk;
      blk = blk->next;
      free(hold);
   }
   conf->entry_pool = NULL;
}

static void config_file_free_owned(config_file_t *conf)
{
   struct config_file_owned_buf *b = conf->owned_bufs;
   while (b)
   {
      struct config_file_owned_buf *hold = b;
      b = b->next;
      if (hold->io)
         hold->io->free_file(hold->data, hold->io->ud);
      else
         free(hold->data);
      free(hold);
   }
   conf->owned_bufs = NULL;
}

/* Forward declaration */
static bool config_file_parse_line(config_file_t *conf,
      struct config_entry_list *list, char *line, config_file_cb_t *cb,
      uint32_t *khash, unsigned p_opts);

static int config_file_sort_compare_func(struct config_entry_list *a,
      struct config_entry_list *b)
{
   if (a && b)
   {
      if (a->key)
      {
         if (b->key)
            return strcasecmp(a->key, b->key);
         return 1;
      }
      else if (b->key)
         return -1;
   }

   return 0;
}

/* https://stackoverflow.com/questions/7685/merge-sort-a-linked-list */
static struct config_entry_list* config_file_merge_sort_linked_list(
         struct config_entry_list *list, int (*compare)(
         struct config_entry_list *one,struct config_entry_list *two))
{
   struct config_entry_list
         *right  = list,
         *temp   = list,
         *last   = list,
         *result = 0,
         *next   = 0,
         *tail   = 0;

   /* Trivial case. */
   if (!list || !list->next)
      return list;

   /* Find halfway through the list (by running two pointers,
    * one at twice the speed of the other). */
   while (temp && temp->next)
   {
      last     = right;
      right    = right->next;
      temp     = temp->next->next;
   }

   /* Break the list in two. (prev pointers are broken here,
    * but we fix later) */
   last->next  = 0;

   /* Recurse on the two smaller lists: */
   list        = config_file_merge_sort_linked_list(list, compare);
   right       = config_file_merge_sort_linked_list(right, compare);

   /* Merge: */
   while (list || right)
   {
      /* Take from empty lists, or compare: */
      if (!right)
      {
         next  = list;
         list  = list->next;
      }
      else if (!list)
      {
         next  = right;
         right = right->next;
      }
      else if (compare(list, right) < 0)
      {
         next  = list;
         list  = list->next;
      }
      else
      {
         next  = right;
         right = right->next;
      }

      if (!result)
         result     = next;
      else
         tail->next = next;

      tail          = next;
   }

   return result;
}

/**
 * config_file_strip_comment:
 *
 * Searches input string for a comment ('#') entry
 * > If first character is '#', then entire line is
 *   a comment and may correspond to a directive
 *   (command action - e.g. include sub-config file).
 *   In this case, 'str' is set to NUL and the comment
 *   itself (everything after the '#' character) is
 *   returned
 * > If a '#' character is found inside a string literal
 *   value, then it does not correspond to a comment and
 *   is ignored. In this case, 'str' is left untouched
 *   and NULL is returned
 * > If a '#' character is found anywhere else, then the
 *   comment text is a suffix of the input string and
 *   has no programmatic value. In this case, the comment
 *   is removed from the end of 'str' and NULL is returned
 **/
static char *config_file_strip_comment(char *str)
{
   /* Search for a comment (#) character */
   char *comment = strchr(str, '#');

   if (comment)
   {
      char *literal_start = NULL;

      /* Check whether entire line is a comment
       * > First character == '#' */
      if (str == comment)
      {
         /* Set 'str' to NUL and return comment
          * for processing at a higher level */
         *str = '\0';
         return ++comment;
      }

      /* Comment character occurs at an offset:
       * Search for the start of a string literal value */
      literal_start = strchr(str, '\"');

      /* Check whether string literal start occurs
       * *before* the comment character */
      if (literal_start && (literal_start < comment))
      {
         /* Search for the end of the string literal
          * value */
         char *literal_end = strchr(literal_start + 1, '\"');

         /* Check whether string literal end occurs
          * *after* the comment character
          * > If this is the case, ignore the comment
          * > Leave 'str' untouched and return NULL */
         if (literal_end && (literal_end > comment))
            return NULL;
      }

      /* If we reach this point, then a comment
       * exists outside of a string literal
       * > Trim the entire comment from the end
       *   of 'str' */
      *comment = '\0';
   }

   return NULL;
}

static char *config_file_extract_value(char *line, unsigned p_opts,
      uint8_t *vflags, size_t *v_len)
{
   while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n')
      line++;

   /* Note: From this point on, an empty value
    * string is valid - and in this case, strldup("", sizeof(""))
    * will be returned (see Note 2)
    * > If we instead return NULL, the the entry
    *   is ignored completely - which means we cannot
    *   track *changes* in entry value */

   /* If first character is ("), we have a full string
    * literal */
   if (*line == '"')
   {
      /* Skip to next character */
      line++;

      /* If this a ("), then value string is empty */
      if (*line != '"')
      {
         /* Find the next (") character.  strchr is the libc's
          * vectorized scan; the previous byte loop walked the
          * value one compare at a time.  An unterminated literal
          * keeps the old semantics: everything to end of line is
          * the value. */
         size_t idx;
         char *end = strchr(line, '\"');
         idx       = end ? (size_t)(end - line) : strlen(line);

         line[idx] = '\0';
         if (idx)
         {
            char *value;
            if (v_len)
               *v_len = idx;
            if (p_opts & CONFIG_FILE_PARSE_BORROW)
            {
               /* The literal is already NUL-terminated in place -
                * the conf owns this buffer, so the entry can point
                * straight at it. */
               *vflags |= CONF_ENTRY_FLG_VAL_BORROWED;
               return line;
            }
            /* Length is known - copy directly instead of strdup
             * re-walking the value to find it again. */
            if ((value = (char*)malloc(idx + 1)))
               memcpy(value, line, idx + 1);
            return value;
         }
      }
   }
   /* This is not a string literal - just read
    * until the next space is found
    * > Note: Skip this if line is empty */
   else if (*line != '\0')
   {
      size_t idx  = 0;
      /* Find next space character */
      while (line[idx] && CONFIG_FILE_ISGRAPH(line[idx]))
         idx++;

      line[idx] = '\0';
      if (idx)
      {
         char *value;
         if (v_len)
            *v_len = idx;
         if (p_opts & CONFIG_FILE_PARSE_BORROW)
         {
            *vflags |= CONF_ENTRY_FLG_VAL_BORROWED;
            return line;
         }
         if ((value = (char*)malloc(idx + 1)))
            memcpy(value, line, idx + 1);
         return value;
      }
   }

   /* Note 2: Return an empty string.
    * calloc gives us a NUL-terminated empty string in one call;
    * borrowed entries share one static instead. */
   if (p_opts & CONFIG_FILE_PARSE_BORROW)
   {
      *vflags |= CONF_ENTRY_FLG_VAL_BORROWED;
      return config_file_empty_value;
   }
   return (char*)calloc(1, 1);
}

/* Move semantics? */
static void config_file_add_child_list(config_file_t *parent,
      config_file_t *child)
{
   struct config_entry_list *list = child->entries;
   bool merge_hash_map            = false;

   /* The child's entries borrow from buffers the child owns; the
    * entries are pilfered below, so the buffers must move with
    * them. */
   if (child->owned_bufs)
   {
      struct config_file_owned_buf *tail = child->owned_bufs;
      while (tail->next)
         tail = tail->next;
      tail->next         = parent->owned_bufs;
      parent->owned_bufs = child->owned_bufs;
      child->owned_bufs  = NULL;
   }
   if (child->entry_pool)
   {
      struct config_file_entry_pool *ptail = child->entry_pool;
      while (ptail->next)
         ptail = ptail->next;
      ptail->next        = parent->entry_pool;
      parent->entry_pool = child->entry_pool;
      child->entry_pool  = NULL;
   }

   /* set list readonly */
   while (list)
   {
      list->readonly = true;
      list           = list->next;
   }

   if (parent->entries)
   {
      /* Use tracked tail instead of walking the list */
      if (parent->tail)
         parent->tail->next = child->entries;
      else
      {
         struct config_entry_list *head = parent->entries;
         while (head->next)
            head = head->next;
         head->next        = child->entries;
      }

      merge_hash_map    = true;
   }
   else
      parent->entries   = child->entries;

   /* Rebase tail. */
   if (parent->entries)
   {
      struct config_entry_list *head =
         (struct config_entry_list*)parent->entries;

      while (head->next)
         head = head->next;
      parent->tail = head;
   }
   else
      parent->tail = NULL;

   /* Update hash map */
   if (merge_hash_map)
   {
      size_t i;
      size_t cap;

      /* We are merging two lists - if any child entry
       * (key) is not present in the parent list, add it
       * to the parent hash map */
      for (i = 0, cap = RHMAP_CAP(child->entries_map); i != cap; i++)
      {
         uint32_t child_hash   = RHMAP_KEY(child->entries_map, i);
         const char *child_key = RHMAP_KEY_STR(child->entries_map, i);

         if (child_hash &&
             child_key &&
             !RHMAP_HAS_FULL(parent->entries_map, child_hash, child_key))
         {
            struct config_entry_list *entry = child->entries_map[i];

            if (entry)
               RHMAP_SET_FULL(parent->entries_map, child_hash, child_key, entry);
         }
      }

      /* Child entries map is no longer required,
       * so free it now */
      RHMAP_FREE(child->entries_map);
   }
   else
   {
      /* If parent list was originally empty,
       * take map from child list */
      RHMAP_FREE(parent->entries_map);
      parent->entries_map = child->entries_map;
      child->entries_map  = NULL;
   }

   child->entries = NULL;
}

static void config_file_get_realpath(char *s, size_t len,
      char *path, const char *config_path)
{
#if !defined(_WIN32) && !defined(__PSL1GHT__) && !defined(__PS3__)
   if (*path == '~')
   {
      const char *home = getenv("HOME");
      if (home)
      {
         size_t _len = strlcpy(s, home,     len);
         strlcpy(s + _len, path + 1, len - _len);
      }
      else
         strlcpy(s, path + 1, len);
   }
   else
#endif
   {
      if (config_path && *config_path)
         fill_pathname_resolve_relative(s, config_path,
            path, len);
      else
         /* No base path to resolve against: use the include
          * path as-is.  Pre-patch this branch left 's'
          * (a stack buffer in config_file_parse_line)
          * uninitialized, so a '#include' directive inside a
          * config parsed via config_file_new_from_string()
          * with a NULL/empty path handed whatever was on the
          * stack to the loader as a file name.  Copying the
          * path verbatim also makes absolute includes work
          * from pathless configs, which is the only kind
          * that can resolve without a base. */
         strlcpy(s, path, len);
   }
}

static void config_file_add_sub_conf(config_file_t *conf, char *path,
      char *s, size_t len, config_file_cb_t *cb)
{
   struct config_include_list *head = conf->includes;
   struct config_include_list *node = (struct config_include_list*)
      malloc(sizeof(*node));

   if (node)
   {
      node->next        = NULL;
      /* Add include list */
      if (!(node->path = strdup(path)))
      {
         free(node);
         goto realpath;
      }

      if (head)
      {
         while (head->next)
            head        = head->next;

         head->next     = node;
      }
      else
         conf->includes = node;
   }

realpath:
   config_file_get_realpath(s, len, path,
         conf->path);
}

size_t config_file_add_reference(config_file_t *conf, char *path)
{
   size_t _len;
   /* It is expected that the conf has it's path already set */
   char short_path[NAME_MAX_LENGTH];
   if (!conf->references)
   {
      conf->references       = (struct path_linked_list*)malloc(sizeof(*conf->references));
      /* NULL-check: the next two field writes NULL-deref on OOM,
       * and the subsequent path_linked_list_add_path call would
       * walk ->next through a NULL head.  On OOM bail before
       * filling short_path - fill_pathname_abbreviated_or_
       * relative returns the computed length regardless of
       * whether references was successfully allocated, so
       * compute-and-return a valid length is also an option, but
       * returning 0 signals 'no reference added' cleanly and
       * matches the state (no reference) that persists. */
      if (!conf->references)
         return 0;
      conf->references->next = NULL;
      conf->references->path = NULL;
   }
   /* A conf parsed from a string may have no path ("It is expected
    * that the conf has it's path already set" was aspiration, not
    * enforcement): a '#reference' directive then handed NULL to
    * fill_pathname_abbreviated_or_relative, whose unconditional
    * strlcpy(buf_b, in_refpath, ...) runs strlen on it - undefined
    * behaviour, found by the differential fuzzer.  With no base
    * path there is nothing to abbreviate against, so record the
    * reference path verbatim - the same resolution fallback the
    * '#include' handler adopted for pathless configs. */
   if (!conf->path)
      _len = strlcpy(short_path, path, sizeof(short_path));
   else
      _len = fill_pathname_abbreviated_or_relative(short_path, conf->path, path, sizeof(short_path));
   path_linked_list_add_path(conf->references, short_path);
   return _len;
}

/**
 * config_file_parse_buffer:
 *
 * Parse a NUL-terminated buffer of config text into @conf, walking
 * it line by line in place (lines are terminated by overwriting the
 * '\n' - the buffer is consumed).  Shared by the path loader, the
 * from-string constructor and the streaming push parser, so the
 * grammar, include/reference handling and callback behaviour cannot
 * drift between entry points.
 *
 * @len is the buffer length if the caller knows it, 0 otherwise;
 * it is only a sizing hint.
 *
 * Returns 0 on success, -1 on allocation failure (the caller owns
 * the buffer and any cleanup of @conf).
 **/
static int config_file_parse_buffer(config_file_t *conf,
      char *buf, size_t len, config_file_cb_t *cb, unsigned p_opts)
{
   char *line = buf;

   /* Pre-size the hash map from the buffer length instead of
    * growing 16 -> 8192 by doubling: every doubling re-hashes and
    * re-inserts all entries placed so far, which the profile put
    * at 6.2% of parse time on a 3300-line config.  One byte of
    * config text per 32 is a deliberate *under*-estimate of the
    * entry count (real lines average ~30 bytes), so a typical load
    * lands within one final grow of the same capacity it would
    * have reached anyway - the map's ceiling is unchanged, only
    * the intermediate re-hash passes go.  Growth failure is fine
    * and needs no checking here: rhmap__grow keeps the old map on
    * OOM, so the map falls back to growing per-insert as before. */
   if (len >= 64)
      RHMAP_FIT(conf->entries_map, len / 32);

   while (line && *line)
   {
      struct config_entry_list *list = NULL;
      uint32_t hash                  = 0;
      uint8_t base_flags             = CONF_ENTRY_FLG_POOLED;
      char *next                     = strchr(line, '\n');
      if (next)
         *next = '\0';               /* terminate this line in place */

      if (*line == '\0')
         goto next_line;

      /* Entry structs come from the conf's pool: entries are only
       * ever freed en masse (deinitialize) or pilfered wholesale,
       * so one bump allocation replaces the per-line malloc and
       * teardown frees blocks instead of walking a free() per
       * entry.  Pool OOM falls back to plain malloc - the entry is
       * then flagged unpooled and freed individually as before. */
      if (!(list = config_file_entry_pool_alloc(conf,
            len ? (len / 32) : 0)))
      {
         base_flags = 0;
         if (!(list = (struct config_entry_list*)malloc(sizeof(*list))))
            return -1;
      }

      list->readonly  = false;
      list->flags     = base_flags;
      list->key       = NULL;
      list->value     = NULL;
      list->key_len   = 0;
      list->value_len = 0;
      list->next      = NULL;

      if (config_file_parse_line(conf, list, line, cb, &hash, p_opts))
      {
         if (conf->entries)
            conf->tail->next = list;
         else
            conf->entries    = list;

         conf->tail = list;

         if (list->key)
         {
            /* Only add entry to the map if an entry with the
             * specified key does not already exist.  'hash' was
             * computed by config_file_parse_line during its key
             * scan - the key bytes are not walked a second time.
             * The previous HAS_FULL-then-SET_FULL pair probed
             * the table twice per insert; PTR_FULL with add=1
             * probes once, returning the existing slot (len
             * unchanged) or claiming a fresh one (len grew), so
             * comparing len before and after distinguishes the
             * two without a second walk.  The slot is written
             * immediately, before any further map call can
             * invalidate the pointer. */
            struct config_entry_list **slot;
            size_t prev_len = RHMAP_LEN(conf->entries_map);
            slot = RHMAP_PTR_FULL(conf->entries_map, hash, list->key);
            if (RHMAP_LEN(conf->entries_map) != prev_len)
            {
               *slot = list;

               if (cb && list->value)
                  cb->config_file_new_entry_cb(list->key, list->value);
            }
         }
      }
      else
      {
         if (list->flags & CONF_ENTRY_FLG_POOLED)
            config_file_entry_pool_unwind(conf);
         else
            free(list);
      }

next_line:
      if (!next)
         break;
      line = next + 1;
   }

   return 0;
}

static int config_file_load_internal(
      struct config_file *conf,
      const char *path, unsigned depth, config_file_cb_t *cb)
{
   /* The config's own interface wins over the process default, so a
    * caller can serve one config (and its includes) from somewhere
    * else without touching global state other threads share. */
   const config_file_io_t *io = conf->io ? conf->io : config_file_io_default;
   int64_t   length    = 0;
   char     *buf       = NULL;
   char     *new_path;
   /* No io interface registered: the core cannot reach files by
    * itself, so a path load reports file-not-found.  See the
    * registration contract in config_file.h. */
   if (!io)
      return 1;
   if (!(new_path = strdup(path)))
      return 1;
   /* Read the whole file once, then walk it line by line in memory.
    * The previous loop fetched each line with filestream_getline,
    * which reads a byte at a time through the VFS (getc == a 1-byte
    * filestream_read): a config of N bytes cost N virtual read calls
    * plus a malloc per line.  A single slurp turns that into one
    * read and an in-place scan - the same load-then-parse split the
    * from_string path already uses - while every line is handed to
    * the identical config_file_parse_line, so include/reference
    * directives, callbacks, and grammar are byte-for-byte
    * unchanged. */
   if (!(buf = io->read_file(path, &length, io->ud)))
   {
      free(new_path);
      return 1;
   }

   conf->path          = new_path;
   conf->include_depth = depth;

   /* Adopt the buffer so entries can borrow key/value strings
    * straight out of it - no per-entry allocation or copy.  On
    * adopt failure (OOM) fall back to the copying parse and free
    * the buffer as before.  Once adopted, teardown owns it on
    * every path, including a mid-parse -1: the caller frees the
    * conf, which releases the buffer after the borrowed pointers
    * into it are dropped. */
   if (config_file_adopt_buffer(conf, buf, io))
   {
      if (config_file_parse_buffer(conf, buf,
            (length > 0) ? (size_t)length : 0, cb,
            CONFIG_FILE_PARSE_BORROW) != 0)
         return -1;
      return 0;
   }

   if (config_file_parse_buffer(conf, buf,
         (length > 0) ? (size_t)length : 0, cb, 0) != 0)
   {
      io->free_file(buf, io->ud);
      free(conf->path);
      conf->path = NULL;
      return -1;
   }

   io->free_file(buf, io->ud);

   return 0;
}

int config_file_load_file(config_file_t *conf, const char *path,
      config_file_cb_t *cb)
{
   return config_file_load_internal(conf, path, 0, cb);
}

static bool config_file_parse_line(config_file_t *conf,
      struct config_entry_list *list, char *line, config_file_cb_t *cb,
      uint32_t *khash, unsigned p_opts)
{
   size_t idx            = 0;
   char *key             = NULL;
   /* Remove any comment text */
   char *comment         = config_file_strip_comment(line);
   /* Check whether entire line is a comment */
   if (comment)
   {
      char *path           = NULL;
      size_t clen          = strlen(comment);
      bool include_found   = clen >= 8  && !memcmp(comment, "include ",   8);
      bool reference_found = clen >= 10 && !memcmp(comment, "reference ", 10);
      /* All comments except those starting with the include or
       * reference directive are ignored */
      if (!include_found && !reference_found)
         return false;
      /* Starting a line with an 'include' directive
       * appends a sub-config file */
      if (include_found)
      {
         config_file_t sub_conf;
         char real_path[PATH_MAX_LENGTH];
         char *include_line = comment + (sizeof("include ")-1);
         if (*include_line == '\0')
            return false;
         if (!(path = config_file_extract_value(include_line, 0, NULL, NULL)))
            return false;
         if (     *path == '\0'
               || conf->include_depth >= MAX_INCLUDE_DEPTH)
         {
            free(path);
            return false;
         }
         config_file_add_sub_conf(conf, path,
            real_path, sizeof(real_path), cb);
         config_file_initialize(&sub_conf);
         /* Includes are fetched the same way their parent was. */
         sub_conf.io = conf->io;
         switch (config_file_load_internal(&sub_conf, real_path,
            conf->include_depth + 1, cb))
         {
            case 0:
               /* Pilfer internal list. */
               config_file_add_child_list(conf, &sub_conf);
               break;
            case -1:
            case 1:
            default:
               break;
         }
         /* Deinitialize on every outcome.  config_file_initialize
          * above allocated the (empty) entries map eagerly - the
          * RHMAP_BORROW_KEYS call forces the map header into
          * existence - so the ret==1 path (include file missing or
          * unreadable) leaked that header, its key table and its
          * key-string table: ~370 bytes per unresolvable #include
          * directive, found by LeakSanitizer.  A game override
          * whose sub-config was deleted leaks it on every load.
          * After a successful pilfer the struct's pointers are
          * NULL/transferred, so the call is equally correct
          * there. */
         config_file_deinitialize(&sub_conf);
      }
      /* Starting a line with an 'reference' directive
       * sets the reference path */
      if (reference_found)
      {
         char *reference_line = comment + (sizeof("reference ")-1);
         if (*reference_line == '\0')
            return false;
         if (!(path = config_file_extract_value(reference_line, 0, NULL, NULL)))
            return false;
         config_file_add_reference(conf, path);
         if (!path)
            return false;
      }
      free(path);
      return true;
   }
   /* Skip to first non-space character */
   while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n')
      line++;
   /* Measure the key span (up to next non-graph char) and hash it
    * with the word-at-a-time internal hash; the classify loop is a
    * compare-only walk with no multiply chain, and the hash then
    * advances four bytes per step over the known span instead of
    * one.  In BORROW mode nothing is copied: the key's terminating
    * NUL is written into the buffer only once the line has proven
    * valid ('=' present, value extracted), overwriting a
    * whitespace/'=' byte the parse has already consumed - so an
    * invalid line bails with the buffer untouched and nothing
    * allocated. */
   {
      const char *key_start = line;
      while (CONFIG_FILE_ISGRAPH(*line))
         line++;
      idx = (size_t)(line - key_start);
      if (idx == 0)
         return false;
      *khash = config_hash_span(key_start, idx);
      /* An entry without a value is invalid */
      while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n')
         line++;
      /* If we don't have an equal sign here,
       * we've got an invalid string. */
      if (*line != '=')
         return false;
      line++;
      {
         size_t value_len = 0;
         if (!(list->value = config_file_extract_value(line, p_opts,
               &list->flags, &value_len)))
            return false;
         list->value_len = config_file_cache_len(value_len);
      }
      list->key_len = config_file_cache_len(idx);
      if (p_opts & CONFIG_FILE_PARSE_BORROW)
      {
         ((char*)key_start)[idx] = '\0';
         list->flags |= CONF_ENTRY_FLG_KEY_BORROWED;
         list->key    = (char*)key_start;
         return true;
      }
      if (!(key = (char*)malloc(idx + 1)))
      {
         if (!(list->flags & CONF_ENTRY_FLG_VAL_BORROWED))
            free(list->value);
         list->value = NULL;
         return false;
      }
      memcpy(key, key_start, idx);
      key[idx]  = '\0';
      list->key = key;
   }
   return true;
}

static int config_from_string_internal(
      struct config_file *conf,
      char *from_string,
      const char *path)
{
   if (path && *path)
      conf->path = strdup(path);
   if (!from_string || !*from_string)
      return 0;
   /* Was a second copy of the line loop, differing from the path
    * loader's only in the (always NULL) callback - two places for
    * the grammar to drift apart.  Both now share
    * config_file_parse_buffer. */
   return config_file_parse_buffer(conf, from_string, 0, NULL, 0);
}

bool config_file_deinitialize(config_file_t *conf)
{
   struct config_include_list *inc_tmp = NULL;
   struct config_entry_list *tmp       = NULL;

   if (!conf)
      return false;

   tmp = conf->entries;
   while (tmp)
   {
      struct config_entry_list *hold = NULL;
      if (tmp->key && !(tmp->flags & CONF_ENTRY_FLG_KEY_BORROWED))
         free(tmp->key);
      if (tmp->value && !(tmp->flags & CONF_ENTRY_FLG_VAL_BORROWED))
         free(tmp->value);

      tmp->value = NULL;
      tmp->key   = NULL;

      hold       = tmp;
      tmp        = tmp->next;

      if (hold && !(hold->flags & CONF_ENTRY_FLG_POOLED))
         free(hold);
   }

   inc_tmp = (struct config_include_list*)conf->includes;
   while (inc_tmp)
   {
      struct config_include_list *hold = NULL;
      if (inc_tmp->path)
         free(inc_tmp->path);
      hold    = (struct config_include_list*)inc_tmp;
      inc_tmp = inc_tmp->next;
      if (hold)
         free(hold);
   }

   path_linked_list_free(conf->references);

   if (conf->path)
      free(conf->path);

   RHMAP_FREE(conf->entries_map);

   /* Borrowed key/value pointers into these buffers were all
    * dropped above, so the backing text and the entry pool can go
    * now. */
   config_file_free_owned(conf);
   config_file_free_pool(conf);

   /* NULL out all pointer fields so that a caller who reuses the
    * struct after deinitialize() -- or who accidentally calls
    * deinitialize() twice -- does not chase dangling pointers.  The
    * free() calls above leave every listed field pointing at freed
    * memory; without these NULLs any subsequent config_* call on
    * this struct is undefined behaviour.  config_file_free() frees
    * the struct itself immediately after this, so for that path the
    * NULLs are redundant but harmless; config_file_deinitialize()
    * is a public API callable on its own. */
   conf->entries     = NULL;
   conf->tail        = NULL;
   conf->last        = NULL;
   conf->includes    = NULL;
   conf->references  = NULL;
   conf->path        = NULL;
   /* entries_map is cleared by RHMAP_FREE */

   return true;
}

/**
 * config_file_free:
 *
 * Frees config file.
 **/
void config_file_free(config_file_t *conf)
{
   if (config_file_deinitialize(conf))
      free(conf);
}

/**
 * config_file_append_conf:
 *
 * Appends the entries of @new_conf to @conf - the merge half of
 * config_append_file (whose path-loading wrapper lives in
 * config_file_io.c), usable directly when the second config was
 * obtained some other way (parsed from a string, streamed in).
 * The key-value pairs of @new_conf take priority over @conf's.
 * Consumes @new_conf.
 **/
bool config_file_append_conf(config_file_t *conf, config_file_t *new_conf)
{
   size_t i, cap;

   if (!new_conf)
      return false;

   /* Update hash map */
   for (i = 0, cap = RHMAP_CAP(new_conf->entries_map); i != cap; i++)
   {
      uint32_t new_hash   = RHMAP_KEY(new_conf->entries_map, i);
      const char *new_key = RHMAP_KEY_STR(new_conf->entries_map, i);

      if (new_hash && new_key)
      {
         struct config_entry_list *entry = new_conf->entries_map[i];

         if (entry)
            RHMAP_SET_FULL(conf->entries_map, new_hash, new_key, entry);
      }
   }

   if (new_conf->tail)
   {
      new_conf->tail->next = conf->entries;
      conf->entries        = new_conf->entries; /* Pilfer. */
      new_conf->entries    = NULL;
   }

   /* Pilfered entries borrow from buffers new_conf owns and live
    * in pool blocks it owns - splice both across before the free
    * below releases them. */
   if (new_conf->owned_bufs)
   {
      struct config_file_owned_buf *tail = new_conf->owned_bufs;
      while (tail->next)
         tail = tail->next;
      tail->next           = conf->owned_bufs;
      conf->owned_bufs     = new_conf->owned_bufs;
      new_conf->owned_bufs = NULL;
   }
   if (new_conf->entry_pool)
   {
      struct config_file_entry_pool *ptail = new_conf->entry_pool;
      while (ptail->next)
         ptail = ptail->next;
      ptail->next          = conf->entry_pool;
      conf->entry_pool     = new_conf->entry_pool;
      new_conf->entry_pool = NULL;
   }

   config_file_free(new_conf);
   return true;
}

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
      const char *path)
{
   struct config_file *conf      = config_file_new_alloc();
   if (     conf
         && config_from_string_internal(
            conf, from_string, path) != -1)
      return conf;
   if (conf)
      config_file_free(conf);
   return NULL;
}

config_file_t *config_file_new_with_io(const char *path,
      const config_file_io_t *io)
{
   struct config_file *conf;
   if (!io)
      return NULL;
   if (!(conf = config_file_new_alloc()))
      return NULL;
   conf->io = io;
   if (!path || !*path)
      return conf;
   if (config_file_load_file(conf, path, NULL) != 0)
   {
      config_file_free(conf);
      return NULL;
   }
   return conf;
}

static config_file_t *config_new_take_string_internal(
      char *from_string, size_t s_len, const char *path,
      const config_file_io_t *io)
{
   struct config_file *conf = config_file_new_alloc();
   if (!conf)
   {
      free(from_string);
      return NULL;
   }
   conf->io = io;
   if (path && *path)
      conf->path = strdup(path);
   if (from_string && *from_string)
   {
      /* Adopt-and-borrow, exactly like the path loaders.  If the
       * adopt node cannot be allocated, degrade to the copying
       * parse and release the string ourselves - ownership was
       * transferred either way. */
      if (config_file_adopt_buffer(conf, from_string, NULL))
      {
         if (config_file_parse_buffer(conf, from_string, s_len,
               NULL, CONFIG_FILE_PARSE_BORROW) != 0)
         {
            config_file_free(conf);
            return NULL;
         }
      }
      else
      {
         int ret = config_file_parse_buffer(conf, from_string, s_len,
               NULL, 0);
         free(from_string);
         if (ret != 0)
         {
            config_file_free(conf);
            return NULL;
         }
      }
   }
   else
      free(from_string);
   return conf;
}

config_file_t *config_file_new_take_string_with_io(char *from_string,
      size_t s_len, const char *path, const config_file_io_t *io)
{
   config_file_t *conf = config_new_take_string_internal(from_string,
         s_len, path, io);
   return conf;
}

config_file_t *config_file_new_take_string(char *from_string,
      size_t s_len, const char *path)
{
   return config_new_take_string_internal(from_string, s_len, path,
         NULL);
}

/* Streaming (push) parser - see the contract in config_file.h.
 *
 * The window buffer accumulates pushed bytes; every push parses the
 * complete lines it can see and slides the unconsumed tail back to
 * the front, so residency is bounded by the longest line plus the
 * largest packet rather than the file.  Bytes are copied exactly
 * once (packet -> window), the same total copy cost as the slurp
 * path's read into its buffer.  Parsing goes through
 * config_file_parse_buffer, the identical line loop behind every
 * other entry point, so streamed output cannot drift from slurped
 * output. */
struct config_file_stream
{
   config_file_t *conf;
   char *win;        /* accumulation window (unparsed tail + incoming) */
   size_t cap;       /* window allocation                              */
   size_t len;       /* unparsed bytes held at win[0..len)             */
   size_t total_in;  /* cumulative pushed bytes (map pre-sizing)       */
   bool oom;
   bool ended;       /* an embedded NUL ended the stream (see push)    */
};

config_file_stream_t *config_file_stream_new(const char *path)
{
   config_file_stream_t *st = (config_file_stream_t*)
         malloc(sizeof(*st));
   if (!st)
      return NULL;
   st->win      = NULL;
   st->cap      = 0;
   st->len      = 0;
   st->total_in = 0;
   st->oom      = false;
   st->ended    = false;
   if (!(st->conf = config_file_new_alloc()))
   {
      free(st);
      return NULL;
   }
   if (path && *path)
   {
      if (!(st->conf->path = strdup(path)))
      {
         config_file_free(st->conf);
         free(st);
         return NULL;
      }
   }
   return st;
}

bool config_file_stream_push(config_file_stream_t *stream,
      const void *data, size_t len)
{
   size_t need;
   size_t cut;
   const char *nl;

   if (!stream || stream->oom)
      return false;
   if (!data || !len || stream->ended)
      return true;

   /* An embedded NUL ends the stream, to keep streamed output
    * structurally identical to handing the same bytes to
    * config_file_new_from_string: the slurp path's line walk
    * cannot see past a NUL (strchr stops there), so it parses the
    * NUL-truncated line as its final line and drops everything
    * after.  Without this clamp the stream would drop only to the
    * end of the current window and then keep parsing later
    * packets - a structural divergence on non-text input, found by
    * inspection and pinned by the differential fuzzer.  Bytes
    * before the NUL still parse; later pushes are accepted and
    * discarded. */
   {
      const char *nulp = (const char*)memchr(data, '\0', len);
      if (nulp)
      {
         len           = (size_t)(nulp - (const char*)data);
         stream->ended = true;
         if (!len)
            return true;
      }
   }

   /* Grow the window to hold tail + packet + NUL */
   need = stream->len + len + 1;
   if (need > stream->cap)
   {
      size_t new_cap = (stream->cap > 0) ? stream->cap : 512;
      char *new_win;
      while (new_cap < need)
      {
         if (new_cap > ((size_t)-1) / 2)
         {
            stream->oom = true;
            return false;
         }
         new_cap *= 2;
      }
      if (!(new_win = (char*)realloc(stream->win, new_cap)))
      {
         stream->oom = true;
         return false;
      }
      stream->win = new_win;
      stream->cap = new_cap;
   }

   memcpy(stream->win + stream->len, data, len);
   stream->len            += len;
   stream->total_in       += len;
   stream->win[stream->len] = '\0';

   /* Parse every complete line in the window.  memrchr is not
    * C89/MSVC, so find the last newline by scanning the packet we
    * just appended backwards - any newline in the retained tail
    * would have been consumed by the push that retained it. */
   cut = stream->len;
   nl  = NULL;
   while (cut > stream->len - len)
   {
      if (stream->win[cut - 1] == '\n')
      {
         nl = &stream->win[cut - 1];
         break;
      }
      cut--;
   }
   if (nl)
   {
      /* Terminate the parseable prefix just past its final
       * newline, remembering the tail byte that NUL displaces.
       * config_file_parse_buffer stops at the NUL; the parsed
       * prefix is then dead and the tail slides to the front. */
      size_t head = (size_t)(nl - stream->win) + 1;
      char saved  = stream->win[head];
      stream->win[head] = '\0';

      /* Pre-size the map from cumulative input; RHMAP_FIT is a
       * no-op once the map is already big enough, so this stays
       * cheap on every push after the first few. */
      if (stream->total_in >= 64)
         RHMAP_FIT(stream->conf->entries_map, stream->total_in / 32);

      if (config_file_parse_buffer(stream->conf, stream->win,
            0, NULL, 0) != 0)
      {
         stream->oom = true;
         return false;
      }

      stream->win[head] = saved;
      stream->len      -= head;
      if (stream->len)
         memmove(stream->win, stream->win + head, stream->len);
      stream->win[stream->len] = '\0';
   }

   return true;
}

config_file_t *config_file_stream_finish(config_file_stream_t *stream)
{
   config_file_t *conf;

   if (!stream)
      return NULL;

   conf = stream->conf;

   /* Parse the final, newline-less tail as its last line */
   if (!stream->oom && stream->len)
   {
      if (config_file_parse_buffer(conf, stream->win, 0, NULL, 0) != 0)
         stream->oom = true;
   }

   if (stream->oom)
   {
      config_file_free(conf);
      conf = NULL;
   }

   free(stream->win);
   free(stream);
   return conf;
}

void config_file_stream_free(config_file_stream_t *stream)
{
   if (!stream)
      return;
   if (stream->conf)
      config_file_free(stream->conf);
   free(stream->win);
   free(stream);
}

/**
 * config_file_initialize:
 *
 * Leaf function.
 **/
void config_file_initialize(struct config_file *conf)
{
   if (!conf)
      return;

   conf->path                     = NULL;
   conf->owned_bufs               = NULL;
   conf->io                       = NULL;
   conf->entry_pool               = NULL;
   conf->entries_map              = NULL;
   conf->entries                  = NULL;
   conf->tail                     = NULL;
   conf->last                     = NULL;
   conf->references               = NULL;
   conf->includes                 = NULL;
   conf->include_depth            = 0;
   conf->flags                    = 0;

   /* Every key handed to the map is the 'key' field of a
    * struct config_entry_list that the config file itself owns and
    * that always outlives the map (config_file_deinitialize tears
    * the entry list down after the map, config_unset removes the
    * map slot before freeing the key, and both config_file_add_
    * child_list and config_append_file pilfer the donor's entries
    * before freeing the donor). Storing a second private copy of
    * every key therefore buys nothing but an allocation per entry
    * on load and a free() per entry on teardown. */
   RHMAP_BORROW_KEYS(conf->entries_map);
}

config_file_t *config_file_new_alloc(void)
{
   struct config_file *conf = (struct config_file*)malloc(sizeof(*conf));
   if (!conf)
      return NULL;
   config_file_initialize(conf);
   return conf;
}

/**
 * config_get_entry_internal:
 *
 * Leaf function.
 **/
static struct config_entry_list *config_get_entry_internal(
      const config_file_t *conf,
      const char *key, struct config_entry_list **prev)
{
   struct config_entry_list *entry = RHMAP_GET_FULL(conf->entries_map, config_hash_span(key, strlen(key)), key);

   if (entry)
      return entry;

   if (prev)
   {
      struct config_entry_list *previous = *prev;
      for (entry = conf->entries; entry; entry = entry->next)
         previous = entry;

      *prev = previous;
   }

   return NULL;
}

struct config_entry_list *config_get_entry(
      const config_file_t *conf, const char *key)
{
   return RHMAP_GET_FULL(conf->entries_map, config_hash_span(key, strlen(key)), key);
}

/**
 * config_get_double:
 *
 * Extracts a double from config file.
 *
 * @return true if found, otherwise false.
 **/
bool config_get_double(config_file_t *conf, const char *key, double *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key);

   if (!entry)
      return false;

   *in = strtod(entry->value, NULL);
   return true;
}

/**
 * config_get_float:
 *
 * Extracts a float from config file.
 *
 * @return true if found, otherwise false.
 **/
bool config_get_float(config_file_t *conf, const char *key, float *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key);

   if (!entry)
      return false;

   /* strtof() is C99/POSIX. Just use the more portable kind. */
   *in = (float)strtod(entry->value, NULL);
   return true;
}

bool config_get_int(config_file_t *conf, const char *key, int *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key);

   if (entry)
   {
      long  val;
      char *end = NULL;
      errno = 0;
      val   = strtol(entry->value, &end, 0);

      if (errno != 0 || end == entry->value || *end != '\0')
         return false;

      if (val < INT_MIN || val > INT_MAX)
         return false;

      *in = (int)val;
      return true;
   }

   return false;
}

bool config_get_size_t(config_file_t *conf, const char *key, size_t *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key);

   if (entry)
   { 
      unsigned long val;
      char *end = NULL;
      errno = 0;
      val   = (unsigned long)strtoul(entry->value, &end, 0);

      if (errno != 0 || end == entry->value || *end != '\0')
         return false;

#if (SIZE_MAX < ULONG_MAX)
      if (val > SIZE_MAX)
         return false;
#endif

      *in = (size_t)val;
      return true;
   }

   return false;
}

#if defined(__STDC_VERSION__) && __STDC_VERSION__>=199901L
bool config_get_uint64(config_file_t *conf, const char *key, uint64_t *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key);

   if (entry)
   {
      uint64_t val;
      char    *end = NULL;
      errno = 0;
      val   = (uint64_t)strtoull(entry->value, &end, 0);

      if (errno != 0 || end == entry->value || *end != '\0')
         return false;

      *in = val;
      return true;
   }
   return false;
}
#endif

bool config_get_uint(config_file_t *conf, const char *key, unsigned *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key);

   if (entry)
   {
      unsigned long  val;
      char          *end = NULL;
      errno = 0;
      val   = strtoul(entry->value, &end, 0);

      if (errno != 0 || end == entry->value || *end != '\0')
         return false;

      if (val > UINT_MAX)
         return false;

      *in = (unsigned)val;
      return true;
   }

   return false;
}

bool config_get_hex(config_file_t *conf, const char *key, unsigned *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key);

   if (entry)
   {
      unsigned long  val;
      char          *end = NULL;
      errno = 0;
      val   = strtoul(entry->value, &end, 16);

      if (errno != 0 || end == entry->value || *end != '\0')
         return false;

      if (val > UINT_MAX)
         return false;

      *in = (unsigned)val;
      return true;
   }

   return false;
}

/**
 * config_get_char:
 *
 * Extracts a single char from config file.
 * If value consists of several chars, this is an error.
 *
 * @return true if found, otherwise false.
 **/
bool config_get_char(config_file_t *conf, const char *key, char *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key);

   if (entry)
   {
      if (entry->value[0] && entry->value[1])
         return false;

      *in = *entry->value;
      return true;
   }

   return false;
}

/**
 * config_get_string:
 *
 * Extracts an allocated string in *in. This must be free()-d if
 * this function succeeds.
 *
 * @return true if found, otherwise false.
 **/
char *config_take_string(config_file_t *conf, const char *key)
{
   char *value                     = NULL;
   struct config_entry_list *entry = config_get_entry(conf, key);

   if (!entry || !entry->value || !*entry->value)
      return NULL;

   if (entry->flags & CONF_ENTRY_FLG_VAL_BORROWED)
   {
      /* The string lives in the conf's adopted buffer: hand the
       * caller a copy it can own. */
      if (!(value = strdup(entry->value)))
         return NULL;
   }
   else
      value = entry->value;

   entry->value     = NULL;
   entry->value_len = 0;
   entry->flags    &= (uint8_t)~CONF_ENTRY_FLG_VAL_BORROWED;
   return value;
}

bool config_get_string(config_file_t *conf, const char *key, char **str)
{
   const struct config_entry_list *entry = config_get_entry(conf, key);
   char *dup;

   if (!entry || !entry->value)
      return false;

   /* strdup can fail; pre-patch the function claimed success with
    * *str possibly left as NULL or uninitialised garbage.  Callers
    * that don't defensively zero *str ahead of the call end up
    * dereferencing a stale pointer. */
   if (!(dup = strdup(entry->value)))
      return false;

   *str = dup;
   return true;
}

/**
  * config_get_config_path:
  *
  * Extracts a string to a preallocated buffer.
  * Avoid memory allocation.
  **/
size_t config_get_config_path(config_file_t *conf, char *s, size_t len)
{
   if (conf)
      return strlcpy(s, conf->path, len);
   return 0;
}

bool config_get_array(config_file_t *conf, const char *key,
      char *s, size_t len)
{
   const struct config_entry_list *entry = config_get_entry(conf, key);
   if (entry)
      return strlcpy(s, entry->value, len) < len;
   return false;
}

bool config_get_path(config_file_t *conf, const char *key,
      char *s, size_t len)
{
#if defined(RARCH_CONSOLE) || !defined(RARCH_INTERNAL)
   return config_get_array(conf, key, s, len);
#else
   const struct config_entry_list *entry = config_get_entry(conf, key);
   if (entry)
   {
      fill_pathname_expand_special(s, entry->value, len);
      return true;
   }
   return false;
#endif
}

/**
 * config_get_bool:
 *
 * Extracts a boolean from config.
 * Valid boolean true are "true" and "1". Valid false are "false" and "0".
 * Other values will be treated as an error.
 *
 * @return true if preconditions are true, otherwise false.
 **/
bool config_get_bool(config_file_t *conf, const char *key, bool *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key);
   if (!entry)
      return false;
   if      (
         entry->value[0] == '1'
      && entry->value[1] == '\0'
         )
      *in = true;
   else if (
         entry->value[0] == 't'
      && entry->value[1] == 'r'
      && entry->value[2] == 'u'
      && entry->value[3] == 'e'
      && entry->value[4] == '\0'
         )
      *in = true;
   else if (
         entry->value[0] == '0'
      && entry->value[1] == '\0'
         )
      *in = false;
   else if (
         entry->value[0] == 'f'
      && entry->value[1] == 'a'
      && entry->value[2] == 'l'
      && entry->value[3] == 's'
      && entry->value[4] == 'e'
      && entry->value[5] == '\0'
         )
      *in = false;
   else
      return false;
   return true;
}

void config_set_string(config_file_t *conf, const char *key, const char *val)
{
   struct config_entry_list *last  = NULL;
   struct config_entry_list *entry = NULL;
   if (!conf || !key || !val)
      return;
   last                            = conf->entries;
   if (conf->flags & CONF_FILE_FLG_GUARANTEED_NO_DUPLICATES)
   {
      if (conf->last)
         last                      = conf->last;
   }
   else
   {
      if ((entry = config_get_entry_internal(conf, key, &last)))
      {
         if (entry->value)
         {
            if (strcmp(entry->value, val) == 0)
               return;
            if (!(entry->flags & CONF_ENTRY_FLG_VAL_BORROWED))
               free(entry->value);
            entry->flags &= (uint8_t)~CONF_ENTRY_FLG_VAL_BORROWED;
         }
         entry->value     = strdup(val);
         entry->value_len = entry->value
               ? config_file_cache_len(strlen(entry->value)) : 0;
         entry->readonly = false;
         conf->flags    |= CONF_FILE_FLG_MODIFIED;
         return;
      }
   }
   if (!(entry = (struct config_entry_list*)malloc(sizeof(*entry))))
      return;
   entry->readonly  = false;
   entry->flags     = 0;
   entry->next      = NULL;
   entry->key_len   = 0;
   entry->value_len = 0;
   entry->key       = strdup(key);
   entry->value     = strdup(val);
   /* If either strdup failed, don't insert a half-initialised entry
    * into the list or hash map -- RHMAP_SET_STR with a NULL key
    * is undefined, and subsequent config_get_string/config_set_*
    * calls on this key would chase a NULL key. */
   if (!entry->key || !entry->value)
   {
      free(entry->key);
      free(entry->value);
      free(entry);
      return;
   }
   entry->key_len   = config_file_cache_len(strlen(entry->key));
   entry->value_len = config_file_cache_len(strlen(entry->value));
   conf->flags     |= CONF_FILE_FLG_MODIFIED;
   if (last)
      last->next    = entry;
   else
      conf->entries = entry;
   conf->last       = entry;
   RHMAP_SET_FULL(conf->entries_map, config_hash_span(entry->key, strlen(entry->key)), entry->key, entry);
}

void config_unset(config_file_t *conf, const char *key)
{
   struct config_entry_list *last  = NULL;
   struct config_entry_list *entry = NULL;

   if (!conf || !key)
      return;

   last  = conf->entries;

   if (!(entry = config_get_entry_internal(conf, key, &last)))
      return;

   (void)RHMAP_DEL_FULL(conf->entries_map, config_hash_span(entry->key, strlen(entry->key)), entry->key);

   if (entry->key && !(entry->flags & CONF_ENTRY_FLG_KEY_BORROWED))
      free(entry->key);

   if (entry->value && !(entry->flags & CONF_ENTRY_FLG_VAL_BORROWED))
      free(entry->value);

   entry->key     = NULL;
   entry->value   = NULL;
   entry->key_len = 0;
   entry->value_len = 0;
   /* Only the string-ownership bits die with the strings: POOLED
    * describes the entry struct itself, which stays in its block
    * (clearing it here made deinitialize free() a pool-interior
    * pointer - caught immediately by ASan in the borrowed-entry
    * lifecycle test). */
   entry->flags  &= (uint8_t)~(CONF_ENTRY_FLG_KEY_BORROWED
                             | CONF_ENTRY_FLG_VAL_BORROWED);
   conf->flags   |= CONF_FILE_FLG_MODIFIED;
}

void config_set_path(config_file_t *conf, const char *entry, const char *val)
{
#if defined(RARCH_CONSOLE) || !defined(RARCH_INTERNAL)
   config_set_string(conf, entry, val);
#else
   char buf[PATH_MAX_LENGTH];
   fill_pathname_abbreviate_special(buf, val, sizeof(buf));
   config_set_string(conf, entry, buf);
#endif
}

size_t config_set_double(config_file_t *conf, const char *key, double val)
{
   char buf[320];
#ifdef __cplusplus
   size_t _len = snprintf(buf, sizeof(buf), "%f", (float)val);
#elif defined(__STDC_VERSION__) && __STDC_VERSION__>=199901L
   size_t _len = snprintf(buf, sizeof(buf), "%lf", val);
#else
   size_t _len = snprintf(buf, sizeof(buf), "%f", (float)val);
#endif
   config_set_string(conf, key, buf);
   return _len;
}

size_t config_set_float(config_file_t *conf, const char *key, float val)
{
   char buf[64];
   size_t _len = snprintf(buf, sizeof(buf), "%f", val);
   config_set_string(conf, key, buf);
   return _len;
}

size_t config_set_int(config_file_t *conf, const char *key, int val)
{
   char buf[16];
   size_t _len = snprintf(buf, sizeof(buf), "%d", val);
   config_set_string(conf, key, buf);
   return _len;
}

size_t config_set_uint(config_file_t *conf, const char *key, unsigned int val)
{
   char buf[16];
   size_t _len = snprintf(buf, sizeof(buf), "%u", val);
   config_set_string(conf, key, buf);
   return _len;
}

size_t config_set_hex(config_file_t *conf, const char *key, unsigned val)
{
   char buf[16];
   size_t _len = snprintf(buf, sizeof(buf), "%x", val);
   config_set_string(conf, key, buf);
   return _len;
}

size_t config_set_uint64(config_file_t *conf, const char *key, uint64_t val)
{
   char buf[32];
   size_t _len = snprintf(buf, sizeof(buf), "%" PRIu64, val);
   config_set_string(conf, key, buf);
   return _len;
}

size_t config_set_char(config_file_t *conf, const char *key, char val)
{
   char buf[2];
   size_t _len = snprintf(buf, sizeof(buf), "%c", val);
   config_set_string(conf, key, buf);
   return _len;
}

/**
 * config_file_dump:
 *
 * Dump the current config to an already opened file.
 * Does not close the file.
 **/
/* Buffered line assembly for config_file_dump.  The previous dump
 * issued two-argument fprintf calls per entry (plus one per
 * reference and include line); with stdio buffering the write
 * itself was cheap, but format parsing per line dominated the save
 * path.  Lines are assembled with memcpy at lengths that are
 * already known and flushed with fwrite; output is byte-identical
 * (no escaping is added - values containing '"' round-trip exactly
 * as lossily as before). */
struct config_file_dump_buf
{
   FILE *file;
   size_t fill;
   char data[4096];
};

static void config_file_dump_flush(struct config_file_dump_buf *b)
{
   if (b->fill)
   {
      fwrite(b->data, 1, b->fill, b->file);
      b->fill = 0;
   }
}

static void config_file_dump_put(struct config_file_dump_buf *b,
      const char *s, size_t _len)
{
   /* Anything larger than the buffer goes out directly */
   if (_len >= sizeof(b->data))
   {
      config_file_dump_flush(b);
      fwrite(s, 1, _len, b->file);
      return;
   }
   if (b->fill + _len > sizeof(b->data))
      config_file_dump_flush(b);
   memcpy(b->data + b->fill, s, _len);
   b->fill += _len;
}

static void config_file_dump_line(struct config_file_dump_buf *b,
      const char *prefix, size_t prefix_len,
      const char *body,   size_t body_len,
      const char *suffix, size_t suffix_len)
{
   config_file_dump_put(b, prefix, prefix_len);
   config_file_dump_put(b, body,   body_len);
   config_file_dump_put(b, suffix, suffix_len);
}

void config_file_dump(config_file_t *conf, FILE *file, bool sort)
{
   /* The dump buffer carries 4 KiB of data inline; heap-held because
    * dumps run from task handlers -- the same lesson config_file_write
    * already learned with its stdio buffer. */
   struct config_file_dump_buf *buf =
      (struct config_file_dump_buf*)malloc(sizeof(*buf));
   struct config_entry_list       *list = NULL;
   struct config_include_list *includes = conf->includes;
   struct path_linked_list *ref_tmp = conf->references;

   if (!buf)
      return;
   buf->file = file;
   buf->fill = 0;

   while (ref_tmp)
   {
      pathname_make_slashes_portable(ref_tmp->path);
      config_file_dump_line(buf,
            "#reference \"", STRLEN_CONST("#reference \""),
            ref_tmp->path, strlen(ref_tmp->path),
            "\"\n", STRLEN_CONST("\"\n"));
      ref_tmp = ref_tmp->next;
   }

   if (sort)
      list = config_file_merge_sort_linked_list(
            (struct config_entry_list*)conf->entries,
            config_file_sort_compare_func);
   else
      list = (struct config_entry_list*)conf->entries;

   conf->entries = list;

   while (list)
   {
      if (!list->readonly && list->key)
      {
         /* Lengths were cached when the strings were parsed or set;
          * zero means unknown and falls back to measuring. */
         config_file_dump_put(buf, list->key,
               list->key_len ? list->key_len : strlen(list->key));
         config_file_dump_put(buf, " = \"", STRLEN_CONST(" = \""));
         config_file_dump_put(buf, list->value,
               list->value_len ? list->value_len : strlen(list->value));
         config_file_dump_put(buf, "\"\n", STRLEN_CONST("\"\n"));
      }
      list = list->next;
   }

   /* Config files are read from the top down - if
    * duplicate entries are found then the topmost
    * one in the list takes precedence. This means
    * '#include' directives must go *after* individual
    * config entries, otherwise they will override
    * any custom-set values */
   while (includes)
   {
      config_file_dump_line(buf,
            "#include \"", STRLEN_CONST("#include \""),
            includes->path, strlen(includes->path),
            "\"\n", STRLEN_CONST("\"\n"));
      includes = includes->next;
   }

   config_file_dump_flush(buf);
   free(buf);
}


/**
 * config_get_entry_list_head:
 *
 * Leaf function.
 **/
bool config_get_entry_list_head(config_file_t *conf,
      struct config_file_entry *entry)
{
   const struct config_entry_list *head = conf->entries;

   if (!head)
      return false;

   entry->key   = head->key;
   entry->value = head->value;
   entry->next  = head->next;
   return true;
}

/**
 * config_get_entry_list_next:
 *
 * Leaf function.
 **/
bool config_get_entry_list_next(struct config_file_entry *entry)
{
   const struct config_entry_list *next = entry->next;

   if (!next)
      return false;

   entry->key   = next->key;
   entry->value = next->value;
   entry->next  = next->next;
   return true;
}

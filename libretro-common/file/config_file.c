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
#include <ctype.h>
#include <errno.h>

#include <retro_miscellaneous.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <compat/fopen_utf8.h>
#include <compat/msvc.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <array/rhmap.h>

#define MAX_INCLUDE_DEPTH 16

struct config_include_list
{
   char *path;
   struct config_include_list *next;
};

/* Forward declaration */
static bool config_file_parse_line(config_file_t *conf,
      struct config_entry_list *list, char *line, config_file_cb_t *cb);

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

static char *config_file_extract_value(char *line)
{
   char *dst = NULL;
   while (ISSPACE((int)*line))
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
      size_t idx  = 0;
      char *value = NULL;
      /* Skip to next character */
      line++;

      /* If this a ("), then value string is empty */
      if (*line != '"')
      {
         /* Find the next (") character */
         while (line[idx] && (line[idx] != '\"'))
            idx++;

         line[idx] = '\0';
         if ((value = line) && *value)
            return strdup(value);
      }
   }
   /* This is not a string literal - just read
    * until the next space is found
    * > Note: Skip this if line is empty */
   else if (*line != '\0')
   {
      size_t idx  = 0;
      char *value = NULL;
      /* Find next space character */
      while (line[idx] && isgraph((int)line[idx]))
         idx++;

      line[idx] = '\0';
      if ((value = line) && *value)
         return strdup(value);
   }

   /* Note 2: This is an unrolled strldup call
    * to avoid an unnecessary dependency -
    * call is strldup("", sizeof(""))
    **/
   dst = (char*)malloc(sizeof(char) * 2);
   strlcpy(dst, "", 1);
   return dst;
}

/* Move semantics? */
static void config_file_add_child_list(config_file_t *parent,
      config_file_t *child)
{
   struct config_entry_list *list = child->entries;
   bool merge_hash_map            = false;

   if (parent->entries)
   {
      struct config_entry_list *head = parent->entries;
      while (head->next)
         head = head->next;

      /* set list readonly */
      while (list)
      {
         list->readonly = true;
         list           = list->next;
      }
      head->next        = child->entries;

      merge_hash_map    = true;
   }
   else
   {
      /* set list readonly */
      while (list)
      {
         list->readonly = true;
         list           = list->next;
      }
      parent->entries   = child->entries;
   }

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
      if (!string_is_empty(config_path))
         fill_pathname_resolve_relative(s, config_path,
            path, len);
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
      node->path        = strdup(path);

      if (head)
      {
         while (head->next)
            head        = head->next;

         head->next     = node;
      }
      else
         conf->includes = node;
   }

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
      conf->references->next = NULL;
      conf->references->path = NULL;
   }
   _len = fill_pathname_abbreviated_or_relative(short_path, conf->path, path, sizeof(short_path));
   path_linked_list_add_path(conf->references, short_path);
   return _len;
}

static int config_file_load_internal(
      struct config_file *conf,
      const char *path, unsigned depth, config_file_cb_t *cb)
{
   RFILE         *file = NULL;
   char      *new_path = strdup(path);
   if (!new_path)
      return 1;

   conf->path          = new_path;
   conf->include_depth = depth;

   if (!(file = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE)))
   {
      free(conf->path);
      return 1;
   }

   while (!filestream_eof(file))
   {
      char *line                     = NULL;
      struct config_entry_list *list = (struct config_entry_list*)
         malloc(sizeof(*list));

      if (!list)
      {
         filestream_close(file);
         return -1;
      }

      list->readonly  = false;
      list->key       = NULL;
      list->value     = NULL;
      list->next      = NULL;

      line            = filestream_getline(file);

      if (!line)
      {
         free(list);
         continue;
      }

      if (
              !string_is_empty(line)
            && config_file_parse_line(conf, list, line, cb))
      {
         if (conf->entries)
            conf->tail->next = list;
         else
            conf->entries    = list;

         conf->tail = list;

         if (list->key)
         {
            /* Only add entry to the map if an entry
             * with the specified value does not
             * already exist */
            uint32_t hash = rhmap_hash_string(list->key);

            if (!RHMAP_HAS_FULL(conf->entries_map, hash, list->key))
            {
               RHMAP_SET_FULL(conf->entries_map, hash, list->key, list);

               if (cb && list->value)
                  cb->config_file_new_entry_cb(list->key, list->value);
            }
         }
      }

      free(line);

      if (list != conf->tail)
         free(list);
   }

   filestream_close(file);

   return 0;
}

static bool config_file_parse_line(config_file_t *conf,
      struct config_entry_list *list, char *line, config_file_cb_t *cb)
{
   size_t cur_size       = 32;
   size_t idx            = 0;
   char *key             = NULL;
   char *key_tmp         = NULL;
   /* Remove any comment text */
   char *comment         = config_file_strip_comment(line);

   /* Check whether entire line is a comment */
   if (comment)
   {
      char *path               = NULL;
      bool include_found       = string_starts_with_size(comment,
            "include ",   STRLEN_CONST("include "));
      bool reference_found     = string_starts_with_size(comment,
            "reference ", STRLEN_CONST("reference "));

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
         char *include_line = comment + STRLEN_CONST("include ");

         if (string_is_empty(include_line))
            return false;

         if (!(path = config_file_extract_value(include_line)))
            return false;

         if (     string_is_empty(path)
               || conf->include_depth >= MAX_INCLUDE_DEPTH)
         {
            free(path);
            return false;
         }

         config_file_add_sub_conf(conf, path,
            real_path, sizeof(real_path), cb);

         config_file_initialize(&sub_conf);

         switch (config_file_load_internal(&sub_conf, real_path,
            conf->include_depth + 1, cb))
         {
            case 0:
               /* Pilfer internal list. */
               config_file_add_child_list(conf, &sub_conf);
               /* fall-through to deinitialize */
            case -1:
               config_file_deinitialize(&sub_conf);
               break;
            case 1:
            default:
               break;
         }
      }

      /* Starting a line with an 'reference' directive
       * sets the reference path */
      if (reference_found)
      {
         char *reference_line = comment + STRLEN_CONST("reference ");

         if (string_is_empty(reference_line))
            return false;

         if (!(path = config_file_extract_value(reference_line)))
            return false;

         config_file_add_reference(conf, path);

         if (!path)
            return false;
      }

      free(path);
      return true;
   }

   /* Skip to first non-space character */
   while (ISSPACE((int)*line))
      line++;

   /* Allocate storage for key */
   if (!(key = (char*)malloc(cur_size + 1)))
      return false;

   /* Copy line contents into key until we
    * reach the next space character */
   while (isgraph((int)*line))
   {
      /* If current key storage is too small,
       * double its size */
      if (idx == cur_size)
      {
         cur_size *= 2;
         if (!(key_tmp   = (char*)realloc(key, cur_size + 1)))
         {
            free(key);
            return false;
         }

         key     = key_tmp;
      }

      key[idx++] = *line++;
   }
   key[idx]      = '\0';

   /* Add key and value entries to list */
   list->key     = key;

   /* An entry without a value is invalid */
   while (ISSPACE((int)*line))
      line++;

   /* If we don't have an equal sign here,
    * we've got an invalid string. */
   if (*line != '=')
   {
      list->value = NULL;
      goto error;
   }

   line++;

   if (!(list->value   = config_file_extract_value(line)))
      goto error;

   return true;

error:
   list->key   = NULL;
   free(key);
   return false;
}

static int config_file_from_string_internal(
      struct config_file *conf,
      char *from_string,
      const char *path)
{
   char *lines                    = from_string;
   char *save_ptr                 = NULL;
   char *line                     = NULL;

   if (!string_is_empty(path))
      conf->path                  = strdup(path);
   if (string_is_empty(lines))
      return 0;

   /* Get first line of config file */
   line = strtok_r(lines, "\n", &save_ptr);

   while (line)
   {
      struct config_entry_list *list = (struct config_entry_list*)
            malloc(sizeof(*list));

      if (!list)
         return -1;

      list->readonly  = false;
      list->key       = NULL;
      list->value     = NULL;
      list->next      = NULL;

      /* Parse current line */
      if (
              !string_is_empty(line)
            && config_file_parse_line(conf, list, line, NULL))
      {
         if (conf->entries)
            conf->tail->next = list;
         else
            conf->entries    = list;

         conf->tail          = list;

         if (list->key)
         {
            /* Only add entry to the map if an entry
             * with the specified value does not
             * already exist */
            uint32_t hash = rhmap_hash_string(list->key);
            if (!RHMAP_HAS_FULL(conf->entries_map, hash, list->key))
               RHMAP_SET_FULL(conf->entries_map, hash, list->key, list);
         }
      }

      if (list != conf->tail)
         free(list);

      /* Get next line of config file */
      line = strtok_r(NULL, "\n", &save_ptr);
   }

   return 0;
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
      if (tmp->key)
         free(tmp->key);
      if (tmp->value)
         free(tmp->value);

      tmp->value = NULL;
      tmp->key   = NULL;

      hold       = tmp;
      tmp        = tmp->next;

      if (hold)
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
 * config_append_file:
 *
 * Loads a new config, and appends its data to @conf.
 * The key-value pairs of the new config file takes priority over the old.
 **/
bool config_append_file(config_file_t *conf, const char *path)
{
   size_t i, cap;
   config_file_t *new_conf = config_file_new_from_path_to_string(path);

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
         && config_file_from_string_internal(
            conf, from_string, path) != -1)
      return conf;
   if (conf)
      config_file_free(conf);
   return NULL;
}

config_file_t *config_file_new_from_path_to_string(const char *path)
{
   if (path_is_valid(path))
   {
	   uint8_t *ret_buf                 = NULL;
      int64_t length                   = 0;
      if (filestream_read_file(path, (void**)&ret_buf, &length))
      {
         config_file_t *conf           = NULL;
         /* Note: 'ret_buf' is not used outside this
          * function - we do not care that it will be
          * modified by config_file_new_from_string() */
         if (length >= 0)
            conf = config_file_new_from_string((char*)ret_buf, path);

         if ((void*)ret_buf)
            free((void*)ret_buf);

         return conf;
      }
   }

   return NULL;
}

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
      const char *path, config_file_cb_t *cb)
{
   int ret                  = 0;
   struct config_file *conf = config_file_new_alloc();
   if (!path || !*path)
      return conf;
   if ((ret = config_file_load_internal(conf, path, 0, cb)) == -1)
   {
      config_file_free(conf);
      return NULL;
   }
   else if (ret == 1)
   {
      free(conf);
      return NULL;
   }
   return conf;
}

/**
 * config_file_new:
 *
 * Loads a config file.
 * If @path is NULL, will create an empty config file.
 *
 * @return Returns NULL if file doesn't exist.
 **/
config_file_t *config_file_new(const char *path)
{
   int ret                  = 0;
   struct config_file *conf = config_file_new_alloc();
   if (!path || !*path)
      return conf;
   if ((ret = config_file_load_internal(conf, path, 0, NULL)) == -1)
   {
      config_file_free(conf);
      return NULL;
   }
   else if (ret == 1)
   {
      free(conf);
      return NULL;
   }
   return conf;
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
   conf->entries_map              = NULL;
   conf->entries                  = NULL;
   conf->tail                     = NULL;
   conf->last                     = NULL;
   conf->references               = NULL;
   conf->includes                 = NULL;
   conf->include_depth            = 0;
   conf->flags                    = 0;
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
   struct config_entry_list *entry = RHMAP_GET_STR(conf->entries_map, key);

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
   return RHMAP_GET_STR(conf->entries_map, key);
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
   errno = 0;

   if (entry)
   {
      int val = (int)strtol(entry->value, NULL, 0);

      if (errno == 0)
      {
         *in = val;
         return true;
      }
   }

   return false;
}

bool config_get_size_t(config_file_t *conf, const char *key, size_t *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key);
   errno = 0;

   if (entry)
   {
      size_t val = 0;
      if (sscanf(entry->value, "%" PRI_SIZET, &val) == 1)
      {
         *in = val;
         return true;
      }
   }

   return false;
}

#if defined(__STDC_VERSION__) && __STDC_VERSION__>=199901L
bool config_get_uint64(config_file_t *conf, const char *key, uint64_t *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key);
   errno = 0;

   if (entry)
   {
      uint64_t val = strtoull(entry->value, NULL, 0);

      if (errno == 0)
      {
         *in = val;
         return true;
      }
   }
   return false;
}
#endif

bool config_get_uint(config_file_t *conf, const char *key, unsigned *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key);
   errno = 0;

   if (entry)
   {
      unsigned val = (unsigned)strtoul(entry->value, NULL, 0);

      if (errno == 0)
      {
         *in = val;
         return true;
      }
   }

   return false;
}

bool config_get_hex(config_file_t *conf, const char *key, unsigned *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key);
   errno = 0;

   if (entry)
   {
      unsigned val = (unsigned)strtoul(entry->value, NULL, 16);

      if (errno == 0)
      {
         *in = val;
         return true;
      }
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
bool config_get_string(config_file_t *conf, const char *key, char **str)
{
   const struct config_entry_list *entry = config_get_entry(conf, key);

   if (!entry || !entry->value)
      return false;

   *str = strdup(entry->value);
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
         (
            entry->value[0] == '1'
         && entry->value[1] == '\0'
         )
         || string_is_equal(entry->value, "true")
         )
      *in = true;
   else if (
         (
            entry->value[0] == '0'
         && entry->value[1] == '\0'
         )
         || string_is_equal(entry->value, "false")
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
         /* An entry corresponding to 'key' already exists
          * > Check whether value is currently set */
         if (entry->value)
         {
            /* Do nothing if value is unchanged */
            if (string_is_equal(entry->value, val))
               return;

            /* Value is to be updated
             * > Free existing */
            free(entry->value);
         }

         /* Update value
          * > Note that once a value is set, it
          *   is no longer considered 'read only' */
         entry->value    = strdup(val);
         entry->readonly = false;
         conf->flags    |= CONF_FILE_FLG_MODIFIED;
         return;
      }
   }

   /* Entry corresponding to 'key' does not exist
    * > Create new entry */
   if (!(entry = (struct config_entry_list*)malloc(sizeof(*entry))))
      return;

   entry->readonly  = false;
   entry->key       = strdup(key);
   entry->value     = strdup(val);
   entry->next      = NULL;
   conf->flags     |= CONF_FILE_FLG_MODIFIED;

   if (last)
      last->next    = entry;
   else
      conf->entries = entry;

   conf->last       = entry;

   RHMAP_SET_STR(conf->entries_map, entry->key, entry);
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

   (void)RHMAP_DEL_STR(conf->entries_map, entry->key);

   if (entry->key)
      free(entry->key);

   if (entry->value)
      free(entry->value);

   entry->key     = NULL;
   entry->value   = NULL;
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
 * config_file_write:
 *
 * Write the current config to a file.
 **/
bool config_file_write(config_file_t *conf, const char *path, bool sort)
{
   if (!conf)
      return false;

   if (conf->flags & CONF_FILE_FLG_MODIFIED)
   {
      if (string_is_empty(path))
         config_file_dump(conf, stdout, sort);
      else
      {
         void* buf  = NULL;
         FILE *file = (FILE*)fopen_utf8(path, "wb");
         if (!file)
            return false;

         buf        = calloc(1, 0x4000);
         setvbuf(file, (char*)buf, _IOFBF, 0x4000);

         config_file_dump(conf, file, sort);

         if (file != stdout)
            fclose(file);
         if (buf)
            free(buf);

         /* Only update modified flag if config file
          * is actually written to disk */
         conf->flags &= ~CONF_FILE_FLG_MODIFIED;
      }
   }

   return true;
}

/**
 * config_file_dump:
 *
 * Dump the current config to an already opened file.
 * Does not close the file.
 **/
void config_file_dump(config_file_t *conf, FILE *file, bool sort)
{
   struct config_entry_list       *list = NULL;
   struct config_include_list *includes = conf->includes;
   struct path_linked_list *ref_tmp = conf->references;

   while (ref_tmp)
   {
      pathname_make_slashes_portable(ref_tmp->path);
      fprintf(file, "#reference \"%s\"\n", ref_tmp->path);
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
         fprintf(file, "%s = \"%s\"\n", list->key, list->value);
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
      fprintf(file, "#include \"%s\"\n", includes->path);
      includes = includes->next;
   }
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

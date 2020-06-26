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

#if !defined(_WIN32) && !defined(__CELLOS_LV2__) && !defined(_XBOX)
#include <sys/param.h> /* PATH_MAX */
#elif defined(_WIN32) && !defined(_XBOX)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(_XBOX)
#include <xtl.h>
#endif
#ifdef ORBIS
#include <sys/fcntl.h>
#include <orbisFile.h>
#endif
#include <retro_miscellaneous.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <compat/fopen_utf8.h>
#include <compat/msvc.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>

#define MAX_INCLUDE_DEPTH 16

struct config_include_list
{
   char *path;
   struct config_include_list *next;
};

static config_file_t *config_file_new_internal(
      const char *path, unsigned depth, config_file_cb_t *cb);

static int config_sort_compare_func(struct config_entry_list *a,
      struct config_entry_list *b)
{
   if (a && b)
   {
      if (a->key && b->key)
         return strcasecmp(a->key, b->key);
      else if (a->key)
         return 1;
      else if (b->key)
         return -1;
   }

   return 0;
}

/* https://stackoverflow.com/questions/7685/merge-sort-a-linked-list */
static struct config_entry_list* merge_sort_linked_list(
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
   list        = merge_sort_linked_list(list, compare);
   right       = merge_sort_linked_list(right, compare);

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

/* Searches input string for a comment ('#') entry
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
 *   is removed from the end of 'str' and NULL is returned */
static char *strip_comment(char *str)
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

static char *extract_value(char *line, bool is_value)
{
   size_t idx  = 0;
   char *value = NULL;

   if (is_value)
   {
      while (isspace((int)*line))
         line++;

      /* If we don't have an equal sign here,
       * we've got an invalid string. */
      if (*line != '=')
         return NULL;

      line++;
   }

   while (isspace((int)*line))
      line++;

   /* Note: From this point on, an empty value
    * string is valid - and in this case, strdup("")
    * will be returned
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
      if (*line == '"')
         return strdup("");

      /* Find the next (") character */
      while (line[idx] && (line[idx] != '\"'))
         idx++;

      line[idx] = '\0';
      value     = line;
   }
   /* This is not a string literal - just read
    * until the next space is found
    * > Note: Skip this if line is empty */
   else if (*line != '\0')
   {
      /* Find next space character */
      while (line[idx] && isgraph((int)line[idx]))
         idx++;

      line[idx] = '\0';
      value     = line;
   }

   if (value && *value)
      return strdup(value);

   return strdup("");
}

/* Move semantics? */
static void add_child_list(config_file_t *parent, config_file_t *child)
{
   struct config_entry_list *list = child->entries;
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

   child->entries = NULL;

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
}

static void add_sub_conf(config_file_t *conf, char *path, config_file_cb_t *cb)
{
   char real_path[PATH_MAX_LENGTH];
   config_file_t         *sub_conf  = NULL;
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

   real_path[0]         = '\0';

#ifdef _WIN32
   if (!string_is_empty(conf->path))
      fill_pathname_resolve_relative(real_path, conf->path,
            path, sizeof(real_path));
#else
#ifndef __CELLOS_LV2__
   if (*path == '~')
   {
      const char *home = getenv("HOME");
      strlcpy(real_path, home ? home : "", sizeof(real_path));
      strlcat(real_path, path + 1, sizeof(real_path));
   }
   else
#endif
      if (!string_is_empty(conf->path))
         fill_pathname_resolve_relative(real_path, conf->path,
               path, sizeof(real_path));
#endif

   sub_conf = (config_file_t*)
      config_file_new_internal(real_path, conf->include_depth + 1, cb);
   if (!sub_conf)
      return;

   /* Pilfer internal list. */
   add_child_list(conf, sub_conf);
   config_file_free(sub_conf);
}

static bool parse_line(config_file_t *conf,
      struct config_entry_list *list, char *line, config_file_cb_t *cb)
{
   size_t cur_size       = 32;
   size_t idx            = 0;
   char *key             = NULL;
   char *key_tmp         = NULL;
   /* Remove any comment text */
   char *comment         = strip_comment(line);

   /* Check whether entire line is a comment */
   if (comment)
   {
      char *path         = NULL;
      char *include_line = NULL;

      /* Starting a line with an 'include' directive
       * appends a sub-config file
       * > All other comments are ignored */
      if (!string_starts_with_size(comment, "include ",
               STRLEN_CONST("include ")))
         return false;

      include_line = comment + STRLEN_CONST("include ");

      if (string_is_empty(include_line))
         return false;

      path = extract_value(include_line, false);

      if (!path)
         return false;

      if (string_is_empty(path))
      {
         free(path);
         return false;
      }

      if (conf->include_depth >= MAX_INCLUDE_DEPTH)
      {
         fprintf(stderr, "!!! #include depth exceeded for config. Might be a cycle.\n");
         free(path);
         return false;
      }

      add_sub_conf(conf, path, cb);
      free(path);
      return true;
   }

   /* Skip to first non-space character */
   while (isspace((int)*line))
      line++;

   /* Allocate storage for key */
   key = (char*)malloc(cur_size + 1);
   if (!key)
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
         key_tmp   = (char*)realloc(key, cur_size + 1);

         if (!key_tmp)
         {
            free(key);
            return false;
         }

         key = key_tmp;
      }

      key[idx++] = *line++;
   }
   key[idx]      = '\0';

   /* Add key and value entries to list */
   list->key     = key;
   list->value   = extract_value(line, true);

   /* An entry without a value is invalid */
   if (!list->value)
   {
      list->key = NULL;
      free(key);
      return false;
   }

   return true;
}

static config_file_t *config_file_new_internal(
      const char *path, unsigned depth, config_file_cb_t *cb)
{
   RFILE              *file = NULL;
   struct config_file *conf = config_file_new_alloc();

   if (!path || !*path)
      return conf;
   conf->path          = strdup(path);
   if (!conf->path)
      goto error;

   conf->include_depth = depth;
   file                = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      free(conf->path);
      goto error;
   }

   while (!filestream_eof(file))
   {
      char *line                     = NULL;
      struct config_entry_list *list = (struct config_entry_list*)
         malloc(sizeof(*list));

      if (!list)
      {
         config_file_free(conf);
         filestream_close(file);
         return NULL;
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
            && parse_line(conf, list, line, cb))
      {
         if (conf->entries)
            conf->tail->next = list;
         else
            conf->entries    = list;

         conf->tail = list;

         if (cb && list->key && list->value)
            cb->config_file_new_entry_cb(list->key, list->value) ;
      }

      free(line);

      if (list != conf->tail)
         free(list);
   }

   filestream_close(file);

   return conf;

error:
   free(conf);

   return NULL;
}

void config_file_free(config_file_t *conf)
{
   struct config_include_list *inc_tmp = NULL;
   struct config_entry_list *tmp       = NULL;
   if (!conf)
      return;

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

   if (conf->path)
      free(conf->path);
   free(conf);
}

bool config_append_file(config_file_t *conf, const char *path)
{
   config_file_t *new_conf = config_file_new_from_path_to_string(path);
   if (!new_conf)
      return false;

   if (new_conf->tail)
   {
      new_conf->tail->next = conf->entries;
      conf->entries        = new_conf->entries; /* Pilfer. */
      new_conf->entries    = NULL;
   }

   config_file_free(new_conf);
   return true;
}

config_file_t *config_file_new_from_string(char *from_string,
      const char *path)
{
   char *lines              = from_string;
   char *save_ptr           = NULL;
   char *line               = NULL;
   struct config_file *conf = (struct config_file*)malloc(sizeof(*conf));

   if (!conf)
      return NULL;

   conf->path                     = NULL;
   conf->entries                  = NULL;
   conf->tail                     = NULL;
   conf->last                     = NULL;
   conf->includes                 = NULL;
   conf->include_depth            = 0;
   conf->guaranteed_no_duplicates = false;
   conf->modified                 = false;

   if (!string_is_empty(path))
      conf->path                  = strdup(path);

   if (string_is_empty(lines))
      return conf;

   /* Get first line of config file */
   line = strtok_r(lines, "\n", &save_ptr);

   while (line)
   {
      struct config_entry_list *list = (struct config_entry_list*)
            malloc(sizeof(*list));

      if (!list)
      {
         config_file_free(conf);
         return NULL;
      }

      list->readonly  = false;
      list->key       = NULL;
      list->value     = NULL;
      list->next      = NULL;

      /* Parse current line */
      if (
              !string_is_empty(line)
            && parse_line(conf, list, line, NULL))
      {
         if (conf->entries)
            conf->tail->next = list;
         else
            conf->entries    = list;

         conf->tail          = list;
      }

      if (list != conf->tail)
         free(list);

      /* Get next line of config file */
      line = strtok_r(NULL, "\n", &save_ptr);
   }

   return conf;
}

config_file_t *config_file_new_from_path_to_string(const char *path)
{
   int64_t length                = 0;
   uint8_t *ret_buf              = NULL;
   config_file_t *conf           = NULL;

   if (path_is_valid(path))
   {
      if (filestream_read_file(path, (void**)&ret_buf, &length))
      {
         /* Note: 'ret_buf' is not used outside this
          * function - we do not care that it will be
          * modified by config_file_new_from_string() */
         if (length >= 0)
            conf = config_file_new_from_string((char*)ret_buf, path);
         if ((void*)ret_buf)
            free((void*)ret_buf);
      }
   }
   return conf;
}

config_file_t *config_file_new_with_callback(
      const char *path, config_file_cb_t *cb)
{
   return config_file_new_internal(path, 0, cb);
}

config_file_t *config_file_new(const char *path)
{
   return config_file_new_internal(path, 0, NULL);
}

config_file_t *config_file_new_alloc(void)
{
   struct config_file *conf = (struct config_file*)malloc(sizeof(*conf));
   if (!conf)
      return NULL;

   conf->path                     = NULL;
   conf->entries                  = NULL;
   conf->tail                     = NULL;
   conf->last                     = NULL;
   conf->includes                 = NULL;
   conf->include_depth            = 0;
   conf->guaranteed_no_duplicates = false;
   conf->modified                 = false;

   return conf;
}

struct config_entry_list *config_get_entry(
      const config_file_t *conf,
      const char *key, struct config_entry_list **prev)
{
   struct config_entry_list *entry    = NULL;
   struct config_entry_list *previous = prev ? *prev : NULL;

   for (entry = conf->entries; entry; entry = entry->next)
   {
      if (string_is_equal(key, entry->key))
         return entry;

      previous = entry;
   }

   if (prev)
      *prev = previous;

   return NULL;
}

bool config_get_double(config_file_t *conf, const char *key, double *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);

   if (!entry)
      return false;

   *in = strtod(entry->value, NULL);
   return true;
}

bool config_get_float(config_file_t *conf, const char *key, float *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);

   if (!entry)
      return false;

   /* strtof() is C99/POSIX. Just use the more portable kind. */
   *in = (float)strtod(entry->value, NULL);
   return true;
}

bool config_get_int(config_file_t *conf, const char *key, int *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);
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
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);
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
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);
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
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);
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
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);
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

bool config_get_char(config_file_t *conf, const char *key, char *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);

   if (entry)
   {
      if (entry->value[0] && entry->value[1])
         return false;

      *in = *entry->value;
      return true;
   }

   return false;
}

bool config_get_string(config_file_t *conf, const char *key, char **str)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);

   if (!entry || !entry->value)
      return false;

   *str = strdup(entry->value);
   return true;
}

bool config_get_config_path(config_file_t *conf, char *s, size_t len)
{
   if (!conf)
      return false;
   return strlcpy(s, conf->path, len);
}

bool config_get_array(config_file_t *conf, const char *key,
      char *buf, size_t size)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);

   if (entry)
      return strlcpy(buf, entry->value, size) < size;
   return false;
}

bool config_get_path(config_file_t *conf, const char *key,
      char *buf, size_t size)
{
#if defined(RARCH_CONSOLE) || !defined(RARCH_INTERNAL)
   if (config_get_array(conf, key, buf, size))
      return true;
#else
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);

   if (entry)
   {
      fill_pathname_expand_special(buf, entry->value, size);
      return true;
   }
#endif
   return false;
}

bool config_get_bool(config_file_t *conf, const char *key, bool *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);

   if (entry)
   {
      if (string_is_equal(entry->value, "true"))
         *in = true;
      else if (string_is_equal(entry->value, "1"))
         *in = true;
      else if (string_is_equal(entry->value, "false"))
         *in = false;
      else if (string_is_equal(entry->value, "0"))
         *in = false;
      else
         return false;
   }

   return entry != NULL;
}

void config_set_string(config_file_t *conf, const char *key, const char *val)
{
   struct config_entry_list *last  = NULL;
   struct config_entry_list *entry = NULL;

   if (!conf || !key || !val)
      return;

   last  = (conf->guaranteed_no_duplicates && conf->last) ?
         conf->last : conf->entries;
   entry = conf->guaranteed_no_duplicates ?
         NULL : config_get_entry(conf, key, &last);

   if (entry)
   {
      /* An entry corresponding to 'key' already exists
       * > Check if it's read only */
      if (entry->readonly)
         return;

      /* Check whether value is currently set */
      if (entry->value)
      {
         /* Do nothing if value is unchanged */
         if (string_is_equal(entry->value, val))
            return;

         /* Value is to be updated
          * > Free existing */
         free(entry->value);
      }

      /* Update value */
      entry->value   = strdup(val);
      conf->modified = true;
      return;
   }

   /* Entry corresponding to 'key' does not exist
    * > Create new entry */
   entry = (struct config_entry_list*)malloc(sizeof(*entry));
   if (!entry)
      return;

   entry->readonly  = false;
   entry->key       = strdup(key);
   entry->value     = strdup(val);
   entry->next      = NULL;
   conf->modified   = true;

   if (last)
      last->next    = entry;
   else
      conf->entries = entry;

   conf->last       = entry;
}

void config_unset(config_file_t *conf, const char *key)
{
   struct config_entry_list *last  = NULL;
   struct config_entry_list *entry = NULL;

   if (!conf || !key)
      return;

   last  = conf->entries;
   entry = config_get_entry(conf, key, &last);

   if (!entry)
      return;

   if (entry->key)
      free(entry->key);

   if (entry->value)
      free(entry->value);

   entry->key     = NULL;
   entry->value   = NULL;
   conf->modified = true;
}

void config_set_path(config_file_t *conf, const char *entry, const char *val)
{
#if defined(RARCH_CONSOLE) || !defined(RARCH_INTERNAL)
   config_set_string(conf, entry, val);
#else
   char buf[PATH_MAX_LENGTH];

   buf[0] = '\0';
   fill_pathname_abbreviate_special(buf, val, sizeof(buf));
   config_set_string(conf, entry, buf);
#endif
}

void config_set_double(config_file_t *conf, const char *key, double val)
{
   char buf[128];

   buf[0] = '\0';
#ifdef __cplusplus
   snprintf(buf, sizeof(buf), "%f", (float)val);
#elif defined(__STDC_VERSION__) && __STDC_VERSION__>=199901L
   snprintf(buf, sizeof(buf), "%lf", val);
#else
   snprintf(buf, sizeof(buf), "%f", (float)val);
#endif
   config_set_string(conf, key, buf);
}

void config_set_float(config_file_t *conf, const char *key, float val)
{
   char buf[128];

   buf[0] = '\0';
   snprintf(buf, sizeof(buf), "%f", val);
   config_set_string(conf, key, buf);
}

void config_set_int(config_file_t *conf, const char *key, int val)
{
   char buf[128];

   buf[0] = '\0';
   snprintf(buf, sizeof(buf), "%d", val);
   config_set_string(conf, key, buf);
}

void config_set_uint(config_file_t *conf, const char *key, unsigned int val)
{
   char buf[128];

   buf[0] = '\0';
   snprintf(buf, sizeof(buf), "%u", val);
   config_set_string(conf, key, buf);
}

void config_set_hex(config_file_t *conf, const char *key, unsigned val)
{
   char buf[128];

   buf[0] = '\0';
   snprintf(buf, sizeof(buf), "%x", val);
   config_set_string(conf, key, buf);
}

void config_set_uint64(config_file_t *conf, const char *key, uint64_t val)
{
   char buf[128];

   buf[0] = '\0';
   snprintf(buf, sizeof(buf), "%" PRIu64, val);
   config_set_string(conf, key, buf);
}

void config_set_char(config_file_t *conf, const char *key, char val)
{
   char buf[2];

   buf[0] = '\0';
   snprintf(buf, sizeof(buf), "%c", val);
   config_set_string(conf, key, buf);
}

void config_set_bool(config_file_t *conf, const char *key, bool val)
{
   config_set_string(conf, key, val ? "true" : "false");
}

bool config_file_write(config_file_t *conf, const char *path, bool sort)
{
   if (!conf)
      return false;

   if (!conf->modified)
      return true;

   if (!string_is_empty(path))
   {
#ifdef ORBIS
      int fd     = orbisOpen(path,O_RDWR|O_CREAT,0644);
      if (fd < 0)
         return false;
      config_file_dump_orbis(conf,fd);
      orbisClose(fd);
#else
      void* buf  = NULL;
      FILE *file = (FILE*)fopen_utf8(path, "wb");
      if (!file)
         return false;

      /* TODO: this is only useful for a few platforms, find which and add ifdef */
#if !defined(PSP)
      buf = calloc(1, 0x4000);
      setvbuf(file, (char*)buf, _IOFBF, 0x4000);
#endif

      config_file_dump(conf, file, sort);

      if (file != stdout)
         fclose(file);
      if (buf)
         free(buf);
#endif

      /* Only update modified flag if config file
       * is actually written to disk */
      conf->modified = false;
   }
   else
      config_file_dump(conf, stdout, sort);

   return true;
}

#ifdef ORBIS
void config_file_dump_orbis(config_file_t *conf, int fd)
{
   struct config_entry_list       *list = NULL;
   struct config_include_list *includes = conf->includes;
   while (includes)
   {
      char cad[256];
      sprintf(cad,"#include %s\n", includes->path);
      orbisWrite(fd, cad, strlen(cad));
      includes = includes->next;
   }

   list = merge_sort_linked_list((struct config_entry_list*)conf->entries, config_sort_compare_func);
   conf->entries = list;

   while (list)
   {
      if (!list->readonly && list->key)
      {
         char newlist[256];
         sprintf(newlist,"%s = %s\n", list->key, list->value);
         orbisWrite(fd, newlist, strlen(newlist));
      }
      list = list->next;
   }
}
#endif

void config_file_dump(config_file_t *conf, FILE *file, bool sort)
{
   struct config_entry_list       *list = NULL;
   struct config_include_list *includes = conf->includes;

   while (includes)
   {
      fprintf(file, "#include \"%s\"\n", includes->path);
      includes = includes->next;
   }

   if (sort)
      list = merge_sort_linked_list((struct config_entry_list*)
            conf->entries, config_sort_compare_func);
   else
      list = (struct config_entry_list*)conf->entries;

   conf->entries = list;

   while (list)
   {
      if (!list->readonly && list->key)
         fprintf(file, "%s = \"%s\"\n", list->key, list->value);
      list = list->next;
   }
}

bool config_entry_exists(config_file_t *conf, const char *entry)
{
   struct config_entry_list *list = conf->entries;

   while (list)
   {
      if (string_is_equal(entry, list->key))
         return true;
      list = list->next;
   }

   return false;
}

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

bool config_file_exists(const char *path)
{
   config_file_t *config = config_file_new(path);
   if (!config)
      return false;

   config_file_free(config);
   return true;
}

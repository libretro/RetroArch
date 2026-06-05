/* Copyright  (C) 2010-2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (core_updater_list.c).
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

#include <file/file_path.h>
#include <string/stdstring.h>
#include <lists/string_list.h>
#include <net/net_http.h>
#include <array/rbuf.h>
#include <retro_miscellaneous.h>

#include "file_path_special.h"
#include "core_info.h"

#include "core_updater_list.h"

/* Holds all entries in a core updater list */
struct core_updater_list
{
   core_updater_list_entry_t *entries;
   enum core_updater_list_type type;
};

/* Cached ('global') core updater list */
static core_updater_list_t *core_list_cached = NULL;

/**************************************/
/* Initialisation / De-Initialisation */
/**************************************/

/* Frees contents of specified core updater
 * list entry */
static void core_updater_list_free_entry(core_updater_list_entry_t *entry)
{
   if (!entry)
      return;

   if (entry->remote_filename)
   {
      free(entry->remote_filename);
      entry->remote_filename = NULL;
   }

   if (entry->remote_core_path)
   {
      free(entry->remote_core_path);
      entry->remote_core_path = NULL;
   }

   if (entry->local_core_path)
   {
      free(entry->local_core_path);
      entry->local_core_path = NULL;
   }

   if (entry->local_info_path)
   {
      free(entry->local_info_path);
      entry->local_info_path = NULL;
   }

   if (entry->display_name)
   {
      free(entry->display_name);
      entry->display_name = NULL;
   }

   if (entry->description)
   {
      free(entry->description);
      entry->description = NULL;
   }

   if (entry->licenses_list)
   {
      string_list_free(entry->licenses_list);
      entry->licenses_list = NULL;
   }
}

/* Creates a new, empty core updater list.
 * Returns a handle to a new core_updater_list_t object
 * on success, otherwise returns NULL. */
core_updater_list_t *core_updater_list_init(void)
{
   /* Create core updater list */
   core_updater_list_t *core_list = (core_updater_list_t*)
         malloc(sizeof(*core_list));

   if (!core_list)
      return NULL;

   /* Initialise members */
   core_list->entries = NULL;
   core_list->type    = CORE_UPDATER_LIST_TYPE_UNKNOWN;

   return core_list;
}

/* Resets (removes all entries of) specified core
 * updater list */
void core_updater_list_reset(core_updater_list_t *core_list)
{
   if (!core_list)
      return;

   if (core_list->entries)
   {
      size_t i;

      for (i = 0; i < RBUF_LEN(core_list->entries); i++)
         core_updater_list_free_entry(&core_list->entries[i]);

      RBUF_FREE(core_list->entries);
   }

   core_list->type = CORE_UPDATER_LIST_TYPE_UNKNOWN;
}

/* Frees specified core updater list */
void core_updater_list_free(core_updater_list_t *core_list)
{
   if (!core_list)
      return;

   core_updater_list_reset(core_list);
   free(core_list);
}

/***************/
/* Cached List */
/***************/

/* Creates a new, empty cached core updater list
 * (i.e. 'global' list).
 * Returns false in the event of an error. */
bool core_updater_list_init_cached(void)
{
   /* Free any existing cached core updater list */
   if (core_list_cached)
   {
      core_updater_list_free(core_list_cached);
      core_list_cached = NULL;
   }

   core_list_cached = core_updater_list_init();

   if (!core_list_cached)
      return false;

   return true;
}

/* Fetches cached core updater list */
core_updater_list_t *core_updater_list_get_cached(void)
{
   if (core_list_cached)
      return core_list_cached;

   return NULL;
}

/* Frees cached core updater list */
void core_updater_list_free_cached(void)
{
   core_updater_list_free(core_list_cached);
   core_list_cached = NULL;
}

/***********/
/* Getters */
/***********/

/* Returns number of entries in core updater list */
size_t core_updater_list_size(core_updater_list_t *core_list)
{
   if (!core_list)
      return 0;

   return RBUF_LEN(core_list->entries);
}

/* Returns 'type' (core delivery method) of
 * specified core updater list */
enum core_updater_list_type core_updater_list_get_type(
      core_updater_list_t *core_list)
{
   if (!core_list)
      return CORE_UPDATER_LIST_TYPE_UNKNOWN;

   return core_list->type;
}

/* Fetches core updater list entry corresponding
 * to the specified entry index.
 * Returns false if index is invalid. */
bool core_updater_list_get_index(
      core_updater_list_t *core_list,
      size_t idx,
      const core_updater_list_entry_t **entry)
{
   if (!core_list || !entry)
      return false;

   if (idx >= RBUF_LEN(core_list->entries))
      return false;

   *entry = &core_list->entries[idx];

   return true;
}

/* Fetches core updater list entry corresponding
 * to the specified remote core filename.
 * Returns false if core is not found. */
bool core_updater_list_get_filename(
      core_updater_list_t *core_list,
      const char *remote_filename,
      const core_updater_list_entry_t **entry)
{
   if (core_list && entry && remote_filename && *remote_filename)
   {
      size_t num_entries = RBUF_LEN(core_list->entries);

      if (num_entries >= 1)
      {
         size_t i;
         /* Search for specified filename */
         for (i = 0; i < num_entries; i++)
         {
            core_updater_list_entry_t *current_entry = &core_list->entries[i];

            if (!current_entry->remote_filename || !*current_entry->remote_filename)
               continue;

            if (string_is_equal(remote_filename,
                current_entry->remote_filename))
            {
               *entry = current_entry;
               return true;
            }
         }
      }
   }

   return false;
}

/* Fetches core updater list entry corresponding
 * to the specified core.
 * Returns false if core is not found. */
bool core_updater_list_get_core(
      core_updater_list_t *core_list,
      const char *local_core_path,
      const core_updater_list_entry_t **entry)
{
   bool resolve_symlinks;
   size_t num_entries;
   size_t i;
   char real_core_path[PATH_MAX_LENGTH];

   if (!core_list || !entry || !local_core_path || !*local_core_path)
      return false;
   if ((num_entries = RBUF_LEN(core_list->entries)) < 1)
      return false;

   /* Resolve absolute pathname of local_core_path */
   strlcpy(real_core_path, local_core_path, sizeof(real_core_path));
   /* Can't resolve symlinks when dealing with cores
    * installed via play feature delivery, because the
    * source files have non-standard file names (which
    * will not be recognised by regular core handling
    * routines) */
   resolve_symlinks = (core_list->type != CORE_UPDATER_LIST_TYPE_PFD);
   path_resolve_realpath(real_core_path, sizeof(real_core_path),
         resolve_symlinks);

   if (!*real_core_path)
      return false;

   /* Search for specified core */
   for (i = 0; i < num_entries; i++)
   {
      core_updater_list_entry_t *current_entry = &core_list->entries[i];

      if (!current_entry->local_core_path || !*current_entry->local_core_path)
         continue;

#ifdef _WIN32
      /* Handle case-insensitive operating systems*/
      if (string_is_equal_noncase(real_core_path,
          current_entry->local_core_path))
      {
#else
      if (string_is_equal(real_core_path,
          current_entry->local_core_path))
      {
#endif
         *entry = current_entry;
         return true;
      }
   }

   return false;
}

/***********/
/* Setters */
/***********/

/* Parses date string and adds contents to
 * specified core updater list entry */
static bool core_updater_list_set_date(
      core_updater_list_entry_t *entry, const char *date_str)
{
   unsigned year, month, day;
   const char *p = date_str;
   char *end;

   if (!entry || !date_str || !*date_str)
      return false;

   year = (unsigned)strtoul(p, &end, 10);
   if (*end != '-')
      return false;
   p = end + 1;

   month = (unsigned)strtoul(p, &end, 10);
   if (*end != '-')
      return false;
   p = end + 1;

   day = (unsigned)strtoul(p, &end, 10);
   if (*end != '\0')
      return false;

   if (month < 1 || month > 12 || day < 1 || day > 31)
      return false;

   entry->date.year  = year;
   entry->date.month = month;
   entry->date.day   = day;

   return true;
}

/* Parses crc string and adds value to
 * specified core updater list entry */
static bool core_updater_list_set_crc(
      core_updater_list_entry_t *entry, const char *crc_str)
{
   uint32_t crc;

   if (!entry || !crc_str || !*crc_str)
      return false;

   if ((crc = (uint32_t)string_hex_to_unsigned(crc_str)) == 0)
      return false;

   entry->crc = crc;

   return true;
}

/* Parses core filename string and adds all
 * associated paths to the specified core
 * updater list entry */
static bool core_updater_list_set_paths(
      core_updater_list_entry_t *entry,
      const char *path_dir_libretro,
      const char *path_libretro_info,
      const char *network_buildbot_url,
      const char *filename_str,
      enum core_updater_list_type list_type)
{
   size_t _len;
   bool is_archive;
   bool resolve_symlinks;
   char remote_core_path[PATH_MAX_LENGTH];
   char local_core_path[PATH_MAX_LENGTH];
   char local_info_path[PATH_MAX_LENGTH];
   char *last_underscore = NULL;

   if (  !entry
       || (!filename_str || !*filename_str)
       || (!path_dir_libretro || !*path_dir_libretro)
       || (!path_libretro_info || !*path_libretro_info))
      return false;

   if (  (list_type == CORE_UPDATER_LIST_TYPE_BUILDBOT)
       && (!network_buildbot_url || !*network_buildbot_url))
      return false;

   is_archive       = path_is_compressed_file(filename_str);
   resolve_symlinks = (list_type != CORE_UPDATER_LIST_TYPE_PFD);

   /* remote_filename - reuse buffer if large enough */
   _len = strlen(filename_str) + 1;
   if (entry->remote_filename)
   {
      char *tmp = (char*)realloc(entry->remote_filename, _len);
      if (!tmp)
         return false;
      entry->remote_filename = tmp;
   }
   else
   {
      entry->remote_filename = (char*)malloc(_len);
      if (!entry->remote_filename)
         return false;
   }
   memcpy(entry->remote_filename, filename_str, _len);

   /* remote_core_path */
   remote_core_path[0] = '\0';
   if (list_type == CORE_UPDATER_LIST_TYPE_BUILDBOT)
   {
      fill_pathname_join_special(
            remote_core_path,
            network_buildbot_url,
            filename_str,
            sizeof(remote_core_path));
      /* URL-encode in place using local_core_path as temp buffer */
      strlcpy(local_core_path, remote_core_path, sizeof(local_core_path));
      remote_core_path[0] = '\0';
      net_http_urlencode_full(
            remote_core_path, local_core_path, sizeof(remote_core_path));
   }

   _len = strlen(remote_core_path) + 1;
   if (entry->remote_core_path)
   {
      char *tmp = (char*)realloc(entry->remote_core_path, _len);
      if (!tmp)
         return false;
      entry->remote_core_path = tmp;
   }
   else
   {
      entry->remote_core_path = (char*)malloc(_len);
      if (!entry->remote_core_path)
         return false;
   }
   memcpy(entry->remote_core_path, remote_core_path, _len);

   /* local_core_path */
   fill_pathname_join_special(
         local_core_path,
         path_dir_libretro,
         filename_str,
         sizeof(local_core_path));

   if (is_archive)
      path_remove_extension(local_core_path);

   path_resolve_realpath(local_core_path, sizeof(local_core_path),
         resolve_symlinks);

   _len = strlen(local_core_path) + 1;
   if (entry->local_core_path)
   {
      char *tmp = (char*)realloc(entry->local_core_path, _len);
      if (!tmp)
         return false;
      entry->local_core_path = tmp;
   }
   else
   {
      entry->local_core_path = (char*)malloc(_len);
      if (!entry->local_core_path)
         return false;
   }
   memcpy(entry->local_core_path, local_core_path, _len);

   /* local_info_path */
   fill_pathname_join_special(
         local_info_path,
         path_libretro_info,
         filename_str,
         sizeof(local_info_path));

   path_remove_extension(local_info_path);

   if (is_archive)
      path_remove_extension(local_info_path);

   last_underscore = (char*)strrchr(local_info_path, '_');
   if (last_underscore && *last_underscore)
      if (memcmp(last_underscore, "_libretro", 9) != 0)
         *last_underscore = '\0';

   _len = strlen(local_info_path);
   strlcpy(
         local_info_path         + _len,
         FILE_PATH_CORE_INFO_EXTENSION,
         sizeof(local_info_path) - _len);

   _len = strlen(local_info_path) + 1;
   if (entry->local_info_path)
   {
      char *tmp = (char*)realloc(entry->local_info_path, _len);
      if (!tmp)
         return false;
      entry->local_info_path = tmp;
   }
   else
   {
      entry->local_info_path = (char*)malloc(_len);
      if (!entry->local_info_path)
         return false;
   }
   memcpy(entry->local_info_path, local_info_path, _len);

   return true;
}

/* Reads info file associated with core and
 * adds relevant information to updater list
 * entry */
static bool core_updater_list_set_core_info(
      core_updater_list_entry_t *entry,
      const char *local_info_path,
      const char *filename_str)
{
   core_updater_info_t *core_info = NULL;

   if (  !entry
       || (!local_info_path || !*local_info_path)
       || (!filename_str || !*filename_str))
      return false;

   /* Clear any existing core info */
   if (entry->display_name)
   {
      free(entry->display_name);
      entry->display_name = NULL;
   }

   if (entry->description)
   {
      free(entry->description);
      entry->description = NULL;
   }

   if (entry->licenses_list)
   {
      /* Note: We can safely leave this as NULL if
       * the core info file is invalid */
      string_list_free(entry->licenses_list);
      entry->licenses_list = NULL;
   }

   entry->is_experimental = false;

   /* Read core info file
    * > Note: It's a bit rubbish that we have to
    *   read the actual core info files here...
    *   Would be better to cache this globally
    *   (at present, we only cache info for
    *    *installed* cores...) */
   if ((core_info = core_info_get_core_updater_info(local_info_path)))
   {
      entry->is_experimental    = (core_info->is_experimental);

      /* display name */
      if (core_info->display_name && *core_info->display_name)
         entry->display_name    = strdup(core_info->display_name);
      else
         entry->display_name    = strdup(filename_str);

      /* description */
      if (core_info->description && *core_info->description)
         entry->description     = strdup(core_info->description);
      else
         entry->description     = strldup("", sizeof(""));

      /* licenses_list */
      if (core_info->licenses && *core_info->licenses)
         entry->licenses_list   = string_split(core_info->licenses, "|");

      /* Clean up */
      core_info_free_core_updater_info(core_info);
   }
   else
   {
      /* If info file is missing, use core filename */
      entry->display_name       = strdup(filename_str);
      entry->description        = strldup("", sizeof(""));
   }

   return true;
}

/* Adds entry to the end of the specified core
 * updater list
 * NOTE: Entry string values are passed by
 * reference - *do not free the entry passed
 * to this function* */
static bool core_updater_list_push_entry(
      core_updater_list_t *core_list, core_updater_list_entry_t *entry)
{
   core_updater_list_entry_t *list_entry = NULL;
   size_t num_entries;

   if (!core_list || !entry)
      return false;

   /* Get current number of list entries */
   num_entries = RBUF_LEN(core_list->entries);

   /* Attempt to allocate memory for new entry */
   if (!RBUF_TRYFIT(core_list->entries, num_entries + 1))
      return false;

   /* Allocation successful - increment array size */
   RBUF_RESIZE(core_list->entries, num_entries + 1);

   /* Get handle of new entry at end of list, and
    * zero-initialise members */
   list_entry = &core_list->entries[num_entries];
   memset(list_entry, 0, sizeof(*list_entry));

   /* Assign paths */
   list_entry->remote_filename  = entry->remote_filename;
   list_entry->remote_core_path = entry->remote_core_path;
   list_entry->local_core_path  = entry->local_core_path;
   list_entry->local_info_path  = entry->local_info_path;

   /* Assign core info */
   list_entry->display_name     = entry->display_name;
   list_entry->description      = entry->description;
   list_entry->licenses_list    = entry->licenses_list;
   list_entry->is_experimental  = entry->is_experimental;

   /* Copy crc */
   list_entry->crc              = entry->crc;

   /* Copy date */
   memcpy(&list_entry->date, &entry->date, sizeof(core_updater_list_date_t));

   return true;
}

/* Parses the contents of a single buildbot
 * core listing and adds it to the specified
 * core updater list */
static void core_updater_list_add_entry(
      core_updater_list_t *core_list,
      const char *path_dir_libretro,
      const char *path_libretro_info,
      const char *network_buildbot_url,
      const char *date_str,
      const char *crc_str,
      const char *filename_str)
{
   const core_updater_list_entry_t *search_entry = NULL;
   core_updater_list_entry_t entry               = {0};

   /* Check whether core file is already included
    * in the list (this is *not* an error condition,
    * it just means we can skip the current listing) */
   if (core_updater_list_get_filename(core_list,
         filename_str, &search_entry))
      goto error;

   /* Parse individual listing strings */
   if (!core_updater_list_set_date(&entry, date_str))
      goto error;

   if (!core_updater_list_set_crc(&entry, crc_str))
      goto error;

   if (!core_updater_list_set_paths(
            &entry,
            path_dir_libretro,
            path_libretro_info,
            network_buildbot_url,
            filename_str,
            CORE_UPDATER_LIST_TYPE_BUILDBOT))
      goto error;

   if (!core_updater_list_set_core_info(
         &entry,
         entry.local_info_path,
         filename_str))
      goto error;

   /* Add entry to list */
   if (!core_updater_list_push_entry(core_list, &entry))
      goto error;

   return;

error:
   /* This is not a *fatal* error - it just
    * means one of the following:
    * - The current line of entry text received
    *   from the buildbot is broken somehow
    *   (could be the case that the network buffer
    *    wasn't large enough to cache the entire
    *    string, so the last line was truncated)
    * - We had insufficient memory to allocate a new
    *   entry in the core updater list
    * In either case, the current entry is discarded
    * and we move on to the next one
    * (network transfers are fishy business, so we
    * choose to ignore this sort of error - don't
    * want the whole fetch to fail because of a
    * trivial glitch...) */
   core_updater_list_free_entry(&entry);
}

/* Core updater list qsort helper function */
static int core_updater_list_qsort_func(
      const core_updater_list_entry_t *a, const core_updater_list_entry_t *b)
{
   if (!a || !b || (!a->display_name || !*a->display_name) || (!b->display_name || !*b->display_name))
      return 0;
   return strcasecmp(a->display_name, b->display_name);
}

/* Sorts core updater list into alphabetical order */
static void core_updater_list_qsort(core_updater_list_t *core_list)
{
   size_t num_entries;

   if (!core_list)
      return;
   if ((num_entries = RBUF_LEN(core_list->entries)) < 2)
      return;
   if (core_list->entries)
      qsort(
         core_list->entries, num_entries,
         sizeof(core_updater_list_entry_t),
         (int (*)(const void *, const void *))
               core_updater_list_qsort_func);
}

/* Reads the contents of a buildbot core list
 * network request into the specified
 * core_updater_list_t object.
 * Returns false in the event of an error. */
bool core_updater_list_parse_network_data(
      core_updater_list_t *core_list,
      const char *path_dir_libretro,
      const char *path_libretro_info,
      const char *network_buildbot_url,
      const char *data, size_t len)
{
   char *data_buf     = NULL;
   char *line         = NULL;
   char *data_end     = NULL;

   /* Sanity check */
   if (!core_list || !data || !*data || (len < 1))
      return false;

   /* We're populating a list 'from scratch' - remove
    * any existing entries */
   core_updater_list_reset(core_list);

   /* Input data string is not terminated - have
    * to copy it to a temporary buffer... */
   if (!(data_buf = (char*)malloc((len + 1) * sizeof(char))))
      return false;

   memcpy(data_buf, data, len * sizeof(char));
   data_buf[len] = '\0';

   data_end = data_buf + len;

   /* Parse each line from the network data */
   for (line = data_buf; line < data_end; )
   {
      char *line_end;
      char *p;
      char *elem0 = NULL; /* date     */
      char *elem1 = NULL; /* crc      */
      char *elem2 = NULL; /* filename */

      /* Find end of current line and terminate it */
      for (line_end = line; line_end < data_end && *line_end != '\n'; line_end++)
         ;
      *line_end = '\0';

      /* Skip empty lines */
      if (!line || !*line)
      {
         line = line_end + 1;
         continue;
      }

      p = line;

      /* --- elem0: date --- */
      /* Skip leading spaces */
      while (*p == ' ')
         p++;
      if (*p != '\0')
      {
         elem0 = p;
         /* Advance to next space and terminate */
         while (*p != ' ' && *p != '\0')
            p++;
         if (*p == ' ')
            *p++ = '\0';
      }

      /* --- elem1: crc --- */
      while (*p == ' ')
         p++;
      if (*p != '\0')
      {
         elem1 = p;
         while (*p != ' ' && *p != '\0')
            p++;
         if (*p == ' ')
            *p++ = '\0';
      }

      /* --- elem2: filename --- */
      while (*p == ' ')
         p++;
      if (*p != '\0')
      {
         elem2 = p;
         while (*p != ' ' && *p != '\0')
            p++;
         if (*p == ' ')
            *p = '\0';
      }

      /* Parse listings info and add to core updater
       * list */
      /* > Listings must have 3 entries:
       *   [date] [crc] [filename] */
      if (     (elem0 && *elem0)
            && (elem1 && *elem1)
            && (elem2 && *elem2))
         core_updater_list_add_entry(
               core_list,
               path_dir_libretro,
               path_libretro_info,
               network_buildbot_url,
               elem0, elem1, elem2);

      /* Advance to next line */
      line = line_end + 1;
   }

   /* Temporary data buffer is no longer required */
   free(data_buf);

   /* Sanity check */
   if (RBUF_LEN(core_list->entries) < 1)
      return false;

   /* Sort completed list */
   core_updater_list_qsort(core_list);

   /* Set list type */
   core_list->type = CORE_UPDATER_LIST_TYPE_BUILDBOT;

   return true;
}

/* Parses a single play feature delivery core
 * listing and adds it to the specified core
 * updater list */
static void core_updater_list_add_pfd_entry(
      core_updater_list_t *core_list,
      const char *path_dir_libretro,
      const char *path_libretro_info,
      const char *filename_str)
{
   const core_updater_list_entry_t *search_entry = NULL;
   core_updater_list_entry_t entry               = {0};

   if (!core_list || !filename_str || !*filename_str)
      goto error;

   /* Check whether core file is already included
    * in the list (this is *not* an error condition,
    * it just means we can skip the current listing) */
   if (core_updater_list_get_filename(core_list,
         filename_str, &search_entry))
      goto error;

   /* Note: Play feature delivery cores have no
    * timestamp or CRC info - leave these fields
    * zero initialised */

   /* Populate entry fields */
   if (!core_updater_list_set_paths(
            &entry,
            path_dir_libretro,
            path_libretro_info,
            NULL,
            filename_str,
            CORE_UPDATER_LIST_TYPE_PFD))
      goto error;

   if (!core_updater_list_set_core_info(
         &entry,
         entry.local_info_path,
         filename_str))
      goto error;

   /* Add entry to list */
   if (!core_updater_list_push_entry(core_list, &entry))
      goto error;

   return;

error:
   /* This is not a *fatal* error - it just
    * means one of the following:
    * - The core listing entry obtained from the
    *   play feature delivery interface is broken
    *   somehow
    * - We had insufficient memory to allocate a new
    *   entry in the core updater list
    * In either case, the current entry is discarded
    * and we move on to the next one */
   core_updater_list_free_entry(&entry);
}

/* Reads the list of cores currently available
 * via play feature delivery (PFD) into the
 * specified core_updater_list_t object.
 * Returns false in the event of an error. */
bool core_updater_list_parse_pfd_data(
      core_updater_list_t *core_list,
      const char *path_dir_libretro,
      const char *path_libretro_info,
      const struct string_list *pfd_cores)
{
   size_t i;

   /* Sanity check */
   if (!core_list || !pfd_cores || (pfd_cores->size < 1))
      return false;

   /* We're populating a list 'from scratch' - remove
    * any existing entries */
   core_updater_list_reset(core_list);

   /* Loop over play feature delivery core list */
   for (i = 0; i < pfd_cores->size; i++)
   {
      const char *filename_str = pfd_cores->elems[i].data;

      if (!filename_str || !*filename_str)
         continue;

      /* Parse core file name and add to core
       * updater list */
      core_updater_list_add_pfd_entry(
            core_list,
            path_dir_libretro,
            path_libretro_info,
            filename_str);
   }

   /* Sanity check */
   if (RBUF_LEN(core_list->entries) < 1)
      return false;

   /* Sort completed list */
   core_updater_list_qsort(core_list);

   /* Set list type */
   core_list->type = CORE_UPDATER_LIST_TYPE_PFD;

   return true;
}

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
   size_t num_entries;
   size_t i;

   if (!core_list || !entry || string_is_empty(remote_filename))
      return false;

   num_entries = RBUF_LEN(core_list->entries);

   if (num_entries < 1)
      return false;

   /* Search for specified filename */
   for (i = 0; i < num_entries; i++)
   {
      core_updater_list_entry_t *current_entry = &core_list->entries[i];

      if (string_is_empty(current_entry->remote_filename))
         continue;

      if (string_is_equal(remote_filename, current_entry->remote_filename))
      {
         *entry = current_entry;
         return true;
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

   real_core_path[0] = '\0';

   if (!core_list || !entry || string_is_empty(local_core_path))
      return false;

   num_entries = RBUF_LEN(core_list->entries);

   if (num_entries < 1)
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

   if (string_is_empty(real_core_path))
      return false;

   /* Search for specified core */
   for (i = 0; i < num_entries; i++)
   {
      core_updater_list_entry_t *current_entry = &core_list->entries[i];

      if (string_is_empty(current_entry->local_core_path))
         continue;

#ifdef _WIN32
      /* Handle case-insensitive operating systems*/
      if (string_is_equal_noncase(real_core_path, current_entry->local_core_path))
      {
#else
      if (string_is_equal(real_core_path, current_entry->local_core_path))
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
   struct string_list date_list = {0};

   if (!entry || string_is_empty(date_str))
      goto error;

   /* Split date string into component values */
   string_list_initialize(&date_list);
   if (!string_split_noalloc(&date_list, date_str, "-"))
         goto error;

   /* Date string must have 3 values:
    * [year] [month] [day] */
   if (date_list.size < 3)
      goto error;

   /* Convert date string values */
   entry->date.year  = string_to_unsigned(date_list.elems[0].data);
   entry->date.month = string_to_unsigned(date_list.elems[1].data);
   entry->date.day   = string_to_unsigned(date_list.elems[2].data);

   /* Clean up */
   string_list_deinitialize(&date_list);

   return true;

error:
   string_list_deinitialize(&date_list);

   return false;
}

/* Parses crc string and adds value to
 * specified core updater list entry */
static bool core_updater_list_set_crc(
      core_updater_list_entry_t *entry, const char *crc_str)
{
   uint32_t crc;

   if (!entry || string_is_empty(crc_str))
      return false;

   crc = (uint32_t)string_hex_to_unsigned(crc_str);

   if (crc == 0)
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
   char *last_underscore                  = NULL;
   char *tmp_url                          = NULL;
   bool is_archive                        = true;
   /* Can't resolve symlinks when dealing with cores
    * installed via play feature delivery, because the
    * source files have non-standard file names (which
    * will not be recognised by regular core handling
    * routines) */
   bool resolve_symlinks                  = (list_type != CORE_UPDATER_LIST_TYPE_PFD);
   char remote_core_path[PATH_MAX_LENGTH];
   char local_core_path[PATH_MAX_LENGTH];
   char local_info_path[PATH_MAX_LENGTH];

   remote_core_path[0] = '\0';
   local_core_path[0]  = '\0';
   local_info_path[0]  = '\0';

   if (!entry ||
       string_is_empty(filename_str) ||
       string_is_empty(path_dir_libretro) ||
       string_is_empty(path_libretro_info))
      return false;

   /* Only buildbot cores require the buildbot URL */
   if ((list_type == CORE_UPDATER_LIST_TYPE_BUILDBOT) &&
       string_is_empty(network_buildbot_url))
      return false;

   /* Check whether remote file is an archive */
   is_archive = path_is_compressed_file(filename_str);

   /* remote_filename */
   if (entry->remote_filename)
   {
      free(entry->remote_filename);
      entry->remote_filename = NULL;
   }

   entry->remote_filename = strdup(filename_str);

   /* remote_core_path
    * > Leave blank if this is not a buildbot core */
   if (list_type == CORE_UPDATER_LIST_TYPE_BUILDBOT)
   {
      fill_pathname_join(
            remote_core_path,
            network_buildbot_url,
            filename_str,
            sizeof(remote_core_path));

      /* > Apply proper URL encoding (messy...) */
      tmp_url = strdup(remote_core_path);
      remote_core_path[0] = '\0';
      net_http_urlencode_full(
            remote_core_path, tmp_url, sizeof(remote_core_path));
      if (tmp_url)
         free(tmp_url);
   }

   if (entry->remote_core_path)
   {
      free(entry->remote_core_path);
      entry->remote_core_path = NULL;
   }

   entry->remote_core_path = strdup(remote_core_path);

   /* local_core_path */
   fill_pathname_join(
         local_core_path,
         path_dir_libretro,
         filename_str,
         sizeof(local_core_path));

   if (is_archive)
      path_remove_extension(local_core_path);

   path_resolve_realpath(local_core_path, sizeof(local_core_path),
         resolve_symlinks);

   if (entry->local_core_path)
   {
      free(entry->local_core_path);
      entry->local_core_path = NULL;
   }

   entry->local_core_path = strdup(local_core_path);

   /* local_info_path */
   fill_pathname_join_noext(
         local_info_path,
         path_libretro_info,
         filename_str,
         sizeof(local_info_path));

   if (is_archive)
      path_remove_extension(local_info_path);

   /* > Remove any non-standard core filename
    *   additions (i.e. info files end with
    *   '_libretro' but core files may have
    *   a platform specific addendum,
    *   e.g. '_android')*/
   last_underscore = (char*)strrchr(local_info_path, '_');

   if (!string_is_empty(last_underscore))
      if (!string_is_equal(last_underscore, "_libretro"))
         *last_underscore = '\0';

   /* > Add proper file extension */
   strlcat(
         local_info_path,
         FILE_PATH_CORE_INFO_EXTENSION,
         sizeof(local_info_path));

   if (entry->local_info_path)
   {
      free(entry->local_info_path);
      entry->local_info_path = NULL;
   }

   entry->local_info_path = strdup(local_info_path);

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

   if (!entry ||
       string_is_empty(local_info_path) ||
       string_is_empty(filename_str))
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
   core_info = core_info_get_core_updater_info(local_info_path);

   if (core_info)
   {
      /* display_name + is_experimental */
      if (!string_is_empty(core_info->display_name))
      {
         entry->display_name    = strdup(core_info->display_name);
         entry->is_experimental = core_info->is_experimental;
      }
      else
      {
         /* If display name is blank, use core filename and
          * assume core is experimental (i.e. all 'fit for consumption'
          * cores must have a valid/complete core info file) */
         entry->display_name    = strdup(filename_str);
         entry->is_experimental = true;
      }

      /* description */
      if (!string_is_empty(core_info->description))
         entry->description     = strdup(core_info->description);
      else
         entry->description     = strdup("");

      /* licenses_list */
      if (!string_is_empty(core_info->licenses))
         entry->licenses_list   = string_split(core_info->licenses, "|");

      /* Clean up */
      core_info_free_core_updater_info(core_info);
   }
   else
   {
      /* If info file is missing, use core filename and
       * assume core is experimental (i.e. all 'fit for consumption'
       * cores must have a valid/complete core info file) */
      entry->display_name       = strdup(filename_str);
      entry->is_experimental    = true;
      entry->description        = strdup("");
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
      struct string_list *network_core_entry_list)
{
   const char *date_str                          = NULL;
   const char *crc_str                           = NULL;
   const char *filename_str                      = NULL;
   const core_updater_list_entry_t *search_entry = NULL;
   core_updater_list_entry_t entry               = {0};

   if (!core_list || !network_core_entry_list)
      goto error;

   /* > Listings must have 3 entries:
    *   [date] [crc] [filename] */
   if (network_core_entry_list->size < 3)
      goto error;

   /* Get handles of the individual listing strings */
   date_str     = network_core_entry_list->elems[0].data;
   crc_str      = network_core_entry_list->elems[1].data;
   filename_str = network_core_entry_list->elems[2].data;

   if (string_is_empty(date_str) ||
       string_is_empty(crc_str) ||
       string_is_empty(filename_str))
      goto error;

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
   if (!a || !b)
      return 0;

   if (string_is_empty(a->display_name) || string_is_empty(b->display_name))
      return 0;

   return strcasecmp(a->display_name, b->display_name);
}

/* Sorts core updater list into alphabetical order */
static void core_updater_list_qsort(core_updater_list_t *core_list)
{
   size_t num_entries;

   if (!core_list)
      return;

   num_entries = RBUF_LEN(core_list->entries);

   if (num_entries < 2)
      return;

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
   size_t i;
   char *data_buf                       = NULL;
   struct string_list network_core_list = {0};

   /* Sanity check */
   if (!core_list || string_is_empty(data) || (len < 1))
      goto error;

   /* We're populating a list 'from scratch' - remove
    * any existing entries */
   core_updater_list_reset(core_list);

   /* Input data string is not terminated - have
    * to copy it to a temporary buffer... */
   data_buf = (char*)malloc((len + 1) * sizeof(char));

   if (!data_buf)
      goto error;

   memcpy(data_buf, data, len * sizeof(char));
   data_buf[len] = '\0';

   /* Split network listing request into lines */
   string_list_initialize(&network_core_list);
   if (!string_split_noalloc(&network_core_list, data_buf, "\n"))
      goto error;

   if (network_core_list.size < 1)
      goto error;

   /* Temporary data buffer is no longer required */
   free(data_buf);
   data_buf = NULL;

   /* Loop over lines */
   for (i = 0; i < network_core_list.size; i++)
   {
      struct string_list network_core_entry_list  = {0};
      const char *line = network_core_list.elems[i].data;

      if (string_is_empty(line))
         continue;

      string_list_initialize(&network_core_entry_list);
      /* Split line into listings info components */
      string_split_noalloc(&network_core_entry_list, line, " ");

      /* Parse listings info and add to core updater
       * list */
      core_updater_list_add_entry(
            core_list,
            path_dir_libretro,
            path_libretro_info,
            network_buildbot_url,
            &network_core_entry_list);

      /* Clean up */
      string_list_deinitialize(&network_core_entry_list);
   }

   /* Sanity check */
   if (RBUF_LEN(core_list->entries) < 1)
      goto error;

   /* Clean up */
   string_list_deinitialize(&network_core_list);

   /* Sort completed list */
   core_updater_list_qsort(core_list);

   /* Set list type */
   core_list->type = CORE_UPDATER_LIST_TYPE_BUILDBOT;

   return true;

error:
   string_list_deinitialize(&network_core_list);

   if (data_buf)
      free(data_buf);

   return false;
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

   if (!core_list || string_is_empty(filename_str))
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

      if (string_is_empty(filename_str))
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

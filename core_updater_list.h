/* Copyright  (C) 2010-2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (core_updater_list.h).
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

#ifndef __CORE_UPDATER_LIST_H
#define __CORE_UPDATER_LIST_H

#include <retro_common_api.h>
#include <libretro.h>

#include <lists/string_list.h>

#include <boolean.h>

RETRO_BEGIN_DECLS

/* Get core updater list */
enum updater_list_status
{
   UPDATER_LIST_BEGIN = 0,
   UPDATER_LIST_WAIT,
   UPDATER_LIST_END
};

/* Defines all possible 'types' of core
 * updater list - corresponds to core
 * delivery method:
 * > Buildbot
 * > Play feature delivery (PFD) */
enum core_updater_list_type
{
   CORE_UPDATER_LIST_TYPE_UNKNOWN = 0,
   CORE_UPDATER_LIST_TYPE_BUILDBOT,
   CORE_UPDATER_LIST_TYPE_PFD
};

/* Holds all date info for a core file
 * on the buildbot */
typedef struct
{
   unsigned year;
   unsigned month;
   unsigned day;
} updater_list_date_t;

/* Holds all info related to a core
 * file on the buildbot */
typedef struct
{
   char *remote_filename;
   char *remote_core_path;
   char *local_core_path;
   char *local_info_path;
   char *display_name;
   char *description;
   struct string_list *licenses_list;
   updater_list_date_t date;   /* unsigned alignment */
   uint32_t crc;
   bool is_experimental;
} core_updater_list_entry_t;

/* Prevent direct access to core_updater_list_t
 * members */
typedef struct core_updater_list core_updater_list_t;


typedef struct
{
   char *remote_filename;
   char *local_filename;
   updater_list_date_t date;   /* unsigned alignment */
   uint32_t crc;
} thumbnail_updater_list_entry_t;

typedef struct thumbnail_updater_list thumbnail_updater_list_t;

/**************************************/
/* Initialisation / De-Initialisation */
/**************************************/

/* Creates a new, empty core updater list.
 * Returns a handle to a new core_updater_list_t object
 * on success, otherwise returns NULL. */
core_updater_list_t *core_updater_list_init(void);

/* Resets (removes all entries of) specified core
 * updater list */
void core_updater_list_reset(core_updater_list_t *core_list);

/* Frees specified core updater list */
void core_updater_list_free(core_updater_list_t *core_list);

/* Thumbnail counterparts */
thumbnail_updater_list_t *thumbnail_updater_list_init(const char *system);
void thumbnail_updater_list_reset(thumbnail_updater_list_t *thumbnail_list);
void thumbnail_updater_list_free(thumbnail_updater_list_t *thumbnail_list);

/***************/
/* Cached List */
/***************/

/* Creates a new, empty cached core updater list
 * (i.e. 'global' list).
 * Returns false in the event of an error. */
bool core_updater_list_init_cached(void);

/* Fetches cached core updater list */
core_updater_list_t *core_updater_list_get_cached(void);

/* Frees cached core updater list */
void core_updater_list_free_cached(void);

/* Thumbnail counterparts */
bool thumbnail_updater_list_init_cached(const char *system);
thumbnail_updater_list_t *thumbnail_updater_list_get_cached(const char *system);
bool thumbnail_updater_list_is_empty(const char* system);

/***********/
/* Getters */
/***********/

/* Returns number of entries in core updater list */
size_t core_updater_list_size(core_updater_list_t *core_list);

/* Returns 'type' (core delivery method) of
 * specified core updater list */
enum core_updater_list_type core_updater_list_get_type(
      core_updater_list_t *core_list);

/* Fetches core updater list entry corresponding
 * to the specified entry index.
 * Returns false if index is invalid. */
bool core_updater_list_get_index(
      core_updater_list_t *core_list,
      size_t idx,
      const core_updater_list_entry_t **entry);

/* Fetches core updater list entry corresponding
 * to the specified remote core filename.
 * Returns false if core is not found. */
bool core_updater_list_get_filename(
      core_updater_list_t *core_list,
      const char *remote_filename,
      const core_updater_list_entry_t **entry);

/* Fetches core updater list entry corresponding
 * to the specified core.
 * Returns false if core is not found. */
bool core_updater_list_get_core(
      core_updater_list_t *core_list,
      const char *local_core_path,
      const core_updater_list_entry_t **entry);

/* Thumbnail equivalents */
size_t thumbnail_updater_list_size(thumbnail_updater_list_t *thumbnail_list);

bool thumbnail_updater_list_get_index(
      thumbnail_updater_list_t *thumbnail_list,
      size_t idx,
      const thumbnail_updater_list_entry_t **entry);

bool thumbnail_updater_list_get_filename(
      thumbnail_updater_list_t *thumbnail_list,
      const char *remote_filename,
      const thumbnail_updater_list_entry_t **entry);

bool thumbnail_updater_list_get_matching_file(
      thumbnail_updater_list_t *thumbnail_list,
      const char *local_thumbnail_path,
      const thumbnail_updater_list_entry_t **entry);

/***********/
/* Setters */
/***********/

/* Reads the contents of a buildbot core list
 * network request into the specified
 * core_updater_list_t object.
 * Returns false in the event of an error. */
bool core_updater_list_parse_network_data(
      core_updater_list_t *core_list,
      const char *path_dir_libretro,
      const char *path_libretro_info,
      const char *network_buildbot_url,
      const char *data, size_t len);

/* Reads the list of cores currently available
 * via play feature delivery (PFD) into the
 * specified core_updater_list_t object.
 * Returns false in the event of an error. */
bool core_updater_list_parse_pfd_data(
      core_updater_list_t *core_list,
      const char *path_dir_libretro,
      const char *path_libretro_info,
      const struct string_list *pfd_cores);

bool thumbnail_updater_list_parse_network_data(
      thumbnail_updater_list_t *thumbnail_list,
      const char *data, size_t len);

RETRO_END_DECLS

#endif

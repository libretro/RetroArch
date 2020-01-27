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

#include <boolean.h>

RETRO_BEGIN_DECLS

/* Default maximum number of entries in
 * a core updater list */
#define CORE_UPDATER_LIST_SIZE 9999

/* Holds all date info for a core file
 * on the buildbot */
typedef struct
{
   unsigned year;
   unsigned month;
   unsigned day;
} core_updater_list_date_t;

/* Holds all info related to a core
 * file on the buildbot */
typedef struct
{
   char *remote_filename;
   char *remote_core_path;
   char *local_core_path;
   char *local_info_path;
   char *display_name;
   uint32_t crc;
   core_updater_list_date_t date;
} core_updater_list_entry_t;

/* Prevent direct access to core_updater_list_t
 * members */
typedef struct core_updater_list core_updater_list_t;

/**************************************/
/* Initialisation / De-Initialisation */
/**************************************/

/* Creates a new, empty core updater list with a
 * maximum number of 'max_size' entries.
 * Returns a handle to a new core_updater_list_t object
 * on success, otherwise returns NULL. */
core_updater_list_t *core_updater_list_init(size_t max_size);

/* Resets (removes all entries of) specified core
 * updater list */
void core_updater_list_reset(core_updater_list_t *core_list);

/* Frees specified core updater list */
void core_updater_list_free(core_updater_list_t *core_list);

/***************/
/* Cached List */
/***************/

/* Creates a new, empty cached core updater list
 * (i.e. 'global' list).
 * Returns false in the event of an error. */
bool core_updater_list_init_cached(size_t max_size);

/* Fetches cached core updater list */
core_updater_list_t *core_updater_list_get_cached(void);

/* Frees cached core updater list */
void core_updater_list_free_cached(void);

/***********/
/* Getters */
/***********/

/* Returns number of entries in core updater list */
size_t core_updater_list_size(core_updater_list_t *core_list);

/* Returns maximum allowed number of entries in core
 * updater list */
size_t core_updater_list_capacity(core_updater_list_t *core_list);

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

RETRO_END_DECLS

#endif

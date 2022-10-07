/* Copyright  (C) 2010-2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (gfx_thumbnail_path.c).
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

#ifndef __GFX_THUMBNAIL_PATH_H
#define __GFX_THUMBNAIL_PATH_H

#include <retro_common_api.h>
#include <libretro.h>

#include <boolean.h>

#include "../playlist.h"

RETRO_BEGIN_DECLS

/* Note: This implementation reflects the current
 * setup of:
 * - menu_driver_set_thumbnail_system()
 * - menu_driver_set_thumbnail_content()
 * - menu_driver_update_thumbnail_path()
 * This is absolutely not the best way to handle things,
 * but I have no interest in rewriting the existing
 * menu code... */

enum gfx_thumbnail_id
{
   GFX_THUMBNAIL_RIGHT = 0,
   GFX_THUMBNAIL_LEFT
};

/* Prevent direct access to gfx_thumbnail_path_data_t members */
typedef struct gfx_thumbnail_path_data gfx_thumbnail_path_data_t;

/* Initialisation */

/* Creates new thumbnail path data container.
 * Returns handle to new gfx_thumbnail_path_data_t object.
 * on success, otherwise NULL.
 * Note: Returned object must be free()d */
gfx_thumbnail_path_data_t *gfx_thumbnail_path_init(void);

/* Resets thumbnail path data
 * (blanks all internal string containers) */
void gfx_thumbnail_path_reset(gfx_thumbnail_path_data_t *path_data);

/* Utility Functions */

/* Fetches the thumbnail subdirectory (Named_Snaps,
 * Named_Titles, Named_Boxarts) corresponding to the
 * specified 'type index' (1, 2, 3).
 * Returns true if 'type index' is valid */
bool gfx_thumbnail_get_sub_directory(unsigned type_idx, const char **sub_directory);

/* Returns true if specified thumbnail is enabled
 * (i.e. if 'type' is not equal to MENU_ENUM_LABEL_VALUE_OFF) */
bool gfx_thumbnail_is_enabled(gfx_thumbnail_path_data_t *path_data, enum gfx_thumbnail_id thumbnail_id);

/* Setters */

/* Sets current 'system' (default database name).
 * Returns true if 'system' is valid.
 * If playlist is provided, extracts system-specific
 * thumbnail assignment metadata (required for accurate
 * usage of gfx_thumbnail_is_enabled())
 * > Used as a fallback when individual content lacks an
 *   associated database name */
bool gfx_thumbnail_set_system(gfx_thumbnail_path_data_t *path_data, const char *system, playlist_t *playlist);

/* Sets current thumbnail content according to the specified label.
 * Returns true if content is valid */
bool gfx_thumbnail_set_content(gfx_thumbnail_path_data_t *path_data, const char *label);

/* Sets current thumbnail content to the specified image.
 * Returns true if content is valid */
bool gfx_thumbnail_set_content_image(gfx_thumbnail_path_data_t *path_data, const char *img_dir, const char *img_name);

/* Sets current thumbnail content to the specified playlist entry.
 * Returns true if content is valid.
 * > Note: It is always best to use playlists when setting
 *   thumbnail content, since there is no guarantee that the
 *   corresponding menu entry label will contain a useful
 *   identifier (it may be 'tainted', e.g. with the current
 *   core name). 'Real' labels should be extracted from source */
bool gfx_thumbnail_set_content_playlist(gfx_thumbnail_path_data_t *path_data, playlist_t *playlist, size_t idx);

/* Updaters */

/* Updates path for specified thumbnail identifier (right, left).
 * Must be called after:
 * - gfx_thumbnail_set_system()
 * - gfx_thumbnail_set_content*()
 * ...and before:
 * - gfx_thumbnail_get_path()
 * Returns true if generated path is valid */
bool gfx_thumbnail_update_path(gfx_thumbnail_path_data_t *path_data, enum gfx_thumbnail_id thumbnail_id);

/* Getters */

/* Fetches the current thumbnail file path of the
 * specified thumbnail 'type'.
 * Returns true if path is valid. */
bool gfx_thumbnail_get_path(gfx_thumbnail_path_data_t *path_data, enum gfx_thumbnail_id thumbnail_id, const char **path);

/* Fetches current 'system' (default database name).
 * Returns true if 'system' is valid. */
bool gfx_thumbnail_get_system(gfx_thumbnail_path_data_t *path_data, const char **system);

/* Fetches current content path.
 * Returns true if content path is valid. */
bool gfx_thumbnail_get_content_path(gfx_thumbnail_path_data_t *path_data, const char **content_path);

/* Fetches current thumbnail label.
 * Returns true if label is valid. */
bool gfx_thumbnail_get_label(gfx_thumbnail_path_data_t *path_data, const char **label);

/* Fetches current thumbnail core name.
 * Returns true if core name is valid. */
bool gfx_thumbnail_get_core_name(gfx_thumbnail_path_data_t *path_data, const char **core_name);

/* Fetches current database name.
 * Returns true if database name is valid. */
bool gfx_thumbnail_get_db_name(gfx_thumbnail_path_data_t *path_data, const char **db_name);

/* Fetches current thumbnail image name
 * (name is the same for all thumbnail types).
 * Returns true if image name is valid. */
bool gfx_thumbnail_get_img_name(gfx_thumbnail_path_data_t *path_data, const char **img_name);

/* Fetches current content directory.
 * Returns true if content directory is valid. */
bool gfx_thumbnail_get_content_dir(gfx_thumbnail_path_data_t *path_data, char *content_dir, size_t len);

/* Fetches current playlist index. */
size_t gfx_thumbnail_get_playlist_index(gfx_thumbnail_path_data_t *path_data);

RETRO_END_DECLS

#endif

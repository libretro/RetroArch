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
#include <retro_miscellaneous.h>

#include "../playlist.h"

RETRO_BEGIN_DECLS

/* Note: This implementation reflects the current
 * setup of:
 * - gfx_thumbnail_set_system()
 * - menu_driver_set_thumbnail_content()
 * - menu_driver_update_thumbnail_path()
 * This is absolutely not the best way to handle things,
 * but I have no interest in rewriting the existing
 * menu code... */

enum gfx_thumbnail_id
{
   GFX_THUMBNAIL_RIGHT = 0,
   GFX_THUMBNAIL_LEFT,
   GFX_THUMBNAIL_ICON
};

/* Prevent direct access to gfx_thumbnail_path_data_t members */
typedef struct gfx_thumbnail_path_data gfx_thumbnail_path_data_t;

/* Used fixed size char arrays here, just to avoid
 * the inconvenience of having to calloc()/free()
 * each individual entry by hand... */
struct gfx_thumbnail_path_data
{
   enum playlist_thumbnail_mode playlist_right_mode;
   enum playlist_thumbnail_mode playlist_left_mode;
   enum playlist_thumbnail_mode playlist_icon_mode;
   size_t playlist_index;
   size_t system_len;
   size_t content_label_len;
   char content_label[NAME_MAX_LENGTH];
   char content_core_name[NAME_MAX_LENGTH];
   char system[NAME_MAX_LENGTH];
   char content_db_name[NAME_MAX_LENGTH];
   char content_path[PATH_MAX_LENGTH];
   char content_img[PATH_MAX_LENGTH];
   char content_img_short[PATH_MAX_LENGTH];
   char content_img_full[PATH_MAX_LENGTH];
   char right_path[PATH_MAX_LENGTH];
   char left_path[PATH_MAX_LENGTH];
   char icon_path[PATH_MAX_LENGTH];
};

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

/* Returns true if specified thumbnail is enabled
 * (i.e. if 'type' is not equal to MENU_ENUM_LABEL_VALUE_OFF) */
bool gfx_thumbnail_is_enabled(gfx_thumbnail_path_data_t *path_data, enum gfx_thumbnail_id thumbnail_id);

/* Setters */

/* Fills content_img field of path_data using existing
 * content_label field (for internal use only) */
void gfx_thumbnail_fill_content_img(char *s, size_t len, const char *src, bool shorten);

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

/* Fetches current content directory.
 * Returns true if content directory is valid. */
size_t gfx_thumbnail_get_content_dir(gfx_thumbnail_path_data_t *path_data, char *s, size_t len);

RETRO_END_DECLS

#endif

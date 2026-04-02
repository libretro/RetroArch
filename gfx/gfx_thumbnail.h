/* Copyright  (C) 2010-2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (gfx_thumbnail.h).
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

#ifndef __GFX_THUMBNAIL_H
#define __GFX_THUMBNAIL_H

#include <retro_common_api.h>
#include <libretro.h>

#include <boolean.h>
#include <retro_miscellaneous.h>

#include "gfx_animation.h"

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

/* Gets the common savestate thumbnail path. */
void gfx_savestate_thumbnail_get_path(char *s, size_t len, const char *state_name, int state_slot);

/* Defines the current status of an entry
 * thumbnail texture */
enum gfx_thumbnail_status
{
   GFX_THUMBNAIL_STATUS_UNKNOWN = 0,
   GFX_THUMBNAIL_STATUS_PENDING,
   GFX_THUMBNAIL_STATUS_AVAILABLE,
   GFX_THUMBNAIL_STATUS_MISSING
};

/* Defines thumbnail alignment within
 * gfx_thumbnail_draw() bounding box */
enum gfx_thumbnail_alignment
{
   GFX_THUMBNAIL_ALIGN_CENTRE = 0,
   GFX_THUMBNAIL_ALIGN_TOP,
   GFX_THUMBNAIL_ALIGN_BOTTOM,
   GFX_THUMBNAIL_ALIGN_LEFT,
   GFX_THUMBNAIL_ALIGN_RIGHT
};

/* Defines all possible thumbnail shadow
 * effect types */
enum gfx_thumbnail_shadow_type
{
   GFX_THUMBNAIL_SHADOW_NONE = 0,
   GFX_THUMBNAIL_SHADOW_DROP,
   GFX_THUMBNAIL_SHADOW_OUTLINE
};

enum gfx_thumbnail_flags
{
   GFX_THUMB_FLAG_FADE_ACTIVE = (1 << 0),
   GFX_THUMB_FLAG_CORE_ASPECT = (1 << 1)
};

/* Holds all runtime parameters associated with
 * an entry thumbnail */
typedef struct
{
   uintptr_t texture;
   unsigned width;
   unsigned height;
   float alpha;
   float delay_timer;
   enum gfx_thumbnail_status status;
   uint8_t flags;
} gfx_thumbnail_t;

/* Holds all configuration parameters associated
 * with a thumbnail shadow effect */
typedef struct
{
   struct
   {
      unsigned width;
   } outline;
   float alpha;
   struct
   {
      float x_offset;
      float y_offset;
   } drop;
   enum gfx_thumbnail_shadow_type type;
} gfx_thumbnail_shadow_t;

/* Structure containing all gfx_thumbnail
 * variables */
struct gfx_thumbnail_state
{
   /* Due to the asynchronous nature of thumbnail
    * loading, it is quite possible to trigger a load
    * then navigate to a different menu list before
    * the load is complete/handled. As an additional
    * safety check, we therefore tag the current menu
    * list with counter value that is incremented whenever
    * a list is cleared/set. This is sent as userdata when
    * requesting a thumbnail, and the upload is only
    * handled if the tag matches the most recent value
    * at the time when the load completes */
   uint64_t list_id;

   /* When streaming thumbnails, to minimise the processing
    * of unnecessary images (i.e. when scrolling rapidly through
    * playlists), we delay loading until an entry has been on screen
    * for at least gfx_thumbnail_delay ms */
   float stream_delay;

   /* Duration in ms of the thumbnail 'fade in' animation */
   float fade_duration;

   /* When true, 'fade in' animation will also be
    * triggered for missing thumbnails */
   bool fade_missing;
};

typedef struct gfx_thumbnail_state gfx_thumbnail_state_t;


/* Setters */

/* When streaming thumbnails, sets time in ms that an
 * entry must be on screen before an image load is
 * requested
 * > if 'delay' is negative, default value is set */
void gfx_thumbnail_set_stream_delay(float delay);

/* Sets duration in ms of the thumbnail 'fade in'
 * animation
 * > If 'duration' is negative, default value is set */
void gfx_thumbnail_set_fade_duration(float duration);

/* Specifies whether 'fade in' animation should be
 * triggered for missing thumbnails
 * > When 'true', allows menu driver to animate
 *   any 'thumbnail unavailable' notifications */
void gfx_thumbnail_set_fade_missing(bool fade_missing);

/* Core interface */

/* When called, prevents the handling of any pending
 * thumbnail load requests
 * >> **MUST** be called before deleting any gfx_thumbnail_t
 *    objects passed to gfx_thumbnail_request() or
 *    gfx_thumbnail_process_stream(), otherwise
 *    heap-use-after-free errors *will* occur */
void gfx_thumbnail_cancel_pending_requests(void);

/* Requests loading of the specified thumbnail
 * - If operation fails, 'thumbnail->status' will be set to
 *   MUI_THUMBNAIL_STATUS_MISSING
 * - If operation is successful, 'thumbnail->status' will be
 *   set to MUI_THUMBNAIL_STATUS_PENDING
 * 'thumbnail' will be populated with texture info/metadata
 * once the image load is complete
 * NOTE 1: Must be called *after* gfx_thumbnail_set_system()
 *         and gfx_thumbnail_set_content*()
 * NOTE 2: 'playlist' and 'idx' are only required here for
 *         on-demand thumbnail download support
 *         (an annoyance...) */ 
void gfx_thumbnail_request(
      gfx_thumbnail_path_data_t *path_data, enum gfx_thumbnail_id thumbnail_id,
      playlist_t *playlist, size_t idx, gfx_thumbnail_t *thumbnail,
      unsigned gfx_thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails);

/* Requests loading of a specific thumbnail image file
 * (may be used, for example, to load savestate images)
 * - If operation fails, 'thumbnail->status' will be set to
 *   MUI_THUMBNAIL_STATUS_MISSING
 * - If operation is successful, 'thumbnail->status' will be
 *   set to MUI_THUMBNAIL_STATUS_PENDING
 * 'thumbnail' will be populated with texture info/metadata
 * once the image load is complete */
void gfx_thumbnail_request_file(
      const char *file_path, gfx_thumbnail_t *thumbnail,
      unsigned gfx_thumbnail_upscale_threshold);

/* Resets (and free()s the current texture of) the
 * specified thumbnail */
void gfx_thumbnail_reset(gfx_thumbnail_t *thumbnail);

/* Stream processing */

/* Requests loading of the specified thumbnail via
 * the stream interface
 * - Must be called on each frame for the duration
 *   that specified thumbnail is on-screen
 * - Actual load request is deferred by currently
 *   set stream delay
 * - Function becomes a no-op once load request is
 *   made
 * - Thumbnails loaded via this function must be
 *   deleted manually via gfx_thumbnail_reset()
 *   when they move off-screen
 * NOTE 1: Must be called *after* gfx_thumbnail_set_system()
 *         and gfx_thumbnail_set_content*()
 * NOTE 2: 'playlist' and 'idx' are only required here for
 *         on-demand thumbnail download support
 *         (an annoyance...)
 * NOTE 3: This function is intended for use in situations
 *         where each menu entry has a *single* thumbnail.
 *         If each entry has two thumbnails, use
 *         gfx_thumbnail_request_streams() for improved
 *         performance */
void gfx_thumbnail_request_stream(
      gfx_thumbnail_path_data_t *path_data,
      gfx_animation_t *p_anim,
      enum gfx_thumbnail_id thumbnail_id,
      playlist_t *playlist, size_t idx,
      gfx_thumbnail_t *thumbnail,
      unsigned gfx_thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails);

/* Requests loading of the specified thumbnails via
 * the stream interface
 * - Must be called on each frame for the duration
 *   that specified thumbnails are on-screen
 * - Actual load request is deferred by currently
 *   set stream delay
 * - Function becomes a no-op once load request is
 *   made
 * - Thumbnails loaded via this function must be
 *   deleted manually via gfx_thumbnail_reset()
 *   when they move off-screen
 * NOTE 1: Must be called *after* gfx_thumbnail_set_system()
 *         and gfx_thumbnail_set_content*()
 * NOTE 2: 'playlist' and 'idx' are only required here for
 *         on-demand thumbnail download support
 *         (an annoyance...)
 * NOTE 3: This function is intended for use in situations
 *         where each menu entry has *two* thumbnails.
 *         If each entry only has a single thumbnail, use
 *         gfx_thumbnail_request_stream() for improved
 *         performance */
void gfx_thumbnail_request_streams(
      gfx_thumbnail_path_data_t *path_data,
      gfx_animation_t *p_anim,
      playlist_t *playlist, size_t idx,
      gfx_thumbnail_t *right_thumbnail,
      gfx_thumbnail_t *left_thumbnail,
      unsigned gfx_thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails);

/* Handles streaming of the specified thumbnail as it moves
 * on/off screen
 * - Must be called each frame for every on-screen entry
 * - Must be called once for each entry as it moves off-screen
 *   (or can be called each frame - overheads are small)
 * NOTE 1: Must be called *after* gfx_thumbnail_set_system()
 * NOTE 2: This function calls gfx_thumbnail_set_content*()
 * NOTE 3: This function is intended for use in situations
 *         where each menu entry has a *single* thumbnail.
 *         If each entry has two thumbnails, use
 *         gfx_thumbnail_process_streams() for improved
 *         performance */
void gfx_thumbnail_process_stream(
      gfx_thumbnail_path_data_t *path_data,
      gfx_animation_t *p_anim,
      enum gfx_thumbnail_id thumbnail_id,
      playlist_t *playlist, size_t idx,
      gfx_thumbnail_t *thumbnail,
      bool on_screen,
      unsigned gfx_thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails);

/* Handles streaming of the specified thumbnails as they move
 * on/off screen
 * - Must be called each frame for every on-screen entry
 * - Must be called once for each entry as it moves off-screen
 *   (or can be called each frame - overheads are small)
 * NOTE 1: Must be called *after* gfx_thumbnail_set_system()
 * NOTE 2: This function calls gfx_thumbnail_set_content*()
 * NOTE 3: This function is intended for use in situations
 *         where each menu entry has *two* thumbnails.
 *         If each entry only has a single thumbnail, use
 *         gfx_thumbnail_process_stream() for improved
 *         performance */
void gfx_thumbnail_process_streams(
      gfx_thumbnail_path_data_t *path_data,
      gfx_animation_t *p_anim,
      playlist_t *playlist, size_t idx,
      gfx_thumbnail_t *right_thumbnail,
      gfx_thumbnail_t *left_thumbnail,
      bool on_screen,
      unsigned gfx_thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails);

/* Thumbnail rendering */

/* Determines the actual screen dimensions of a
 * thumbnail when centred with aspect correct
 * scaling within a rectangle of (width x height) */
void gfx_thumbnail_get_draw_dimensions(
      gfx_thumbnail_t *thumbnail,
      unsigned width, unsigned height, float scale_factor,
      float *draw_width, float *draw_height);

/* Draws specified thumbnail with specified alignment
 * (and aspect correct scaling) within a rectangle of
 * (width x height).
 * 'shadow' defines an optional shadow effect (may be
 * set to NULL if a shadow effect is not required).
 * NOTE: Setting scale_factor > 1.0f will increase the
 *       size of the thumbnail beyond the limits of the
 *       (width x height) rectangle (alignment + aspect
 *       correct scaling is preserved). Use with caution */
void gfx_thumbnail_draw(
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      gfx_thumbnail_t *thumbnail,
      float x, float y, unsigned width, unsigned height,
      enum gfx_thumbnail_alignment alignment,
      float alpha, float scale_factor,
      gfx_thumbnail_shadow_t *shadow);

gfx_thumbnail_state_t *gfx_thumb_get_ptr(void);

RETRO_END_DECLS

#endif

/* Copyright  (C) 2010-2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (gfx_thumbnail.c).
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

#include "gfx_animation.h"
#include "gfx_thumbnail_path.h"

RETRO_BEGIN_DECLS

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
   bool fade_active;
   bool core_aspect;
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

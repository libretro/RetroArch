/* Copyright  (C) 2010-2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (menu_thumbnail.c).
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

#ifndef __MENU_THUMBNAIL_H
#define __MENU_THUMBNAIL_H

#include <retro_common_api.h>
#include <libretro.h>

#include <boolean.h>

#include "menu_thumbnail_path.h"

RETRO_BEGIN_DECLS

/* Defines the current status of an entry
 * thumbnail texture */
enum menu_thumbnail_status
{
   MENU_THUMBNAIL_STATUS_UNKNOWN = 0,
   MENU_THUMBNAIL_STATUS_PENDING,
   MENU_THUMBNAIL_STATUS_AVAILABLE,
   MENU_THUMBNAIL_STATUS_MISSING
};

/* Holds all runtime parameters associated with
 * an entry thumbnail */
typedef struct
{
   enum menu_thumbnail_status status;
   uintptr_t texture;
   unsigned width;
   unsigned height;
   float alpha;
   float delay_timer;
} menu_thumbnail_t;

/* Setters */

/* When streaming thumbnails, sets time in ms that an
 * entry must be on screen before an image load is
 * requested */
void menu_thumbnail_set_stream_delay(float delay);

/* Sets duration in ms of the thumbnail 'fade in'
 * animation */
void menu_thumbnail_set_fade_duration(float duration);

/* Getters */

/* Fetches current streaming thumbnails request delay */
float menu_thumbnail_get_stream_delay(void);

/* Fetches current 'fade in' animation duration */
float menu_thumbnail_get_fade_duration(void);

/* Core interface */

/* When called, prevents the handling of any pending
 * thumbnail load requests
 * >> **MUST** be called before deleting any menu_thumbnail_t
 *    objects passed to menu_thumbnail_request() or
 *    menu_thumbnail_process_stream(), otherwise
 *    heap-use-after-free errors *will* occur */
void menu_thumbnail_cancel_pending_requests(void);

/* Requests loading of the specified thumbnail
 * - If operation fails, 'thumbnail->status' will be set to
 *   MUI_THUMBNAIL_STATUS_MISSING
 * - If operation is successful, 'thumbnail->status' will be
 *   set to MUI_THUMBNAIL_STATUS_PENDING
 * 'thumbnail' will be populated with texture info/metadata
 * once the image load is complete
 * NOTE 1: Must be called *after* menu_thumbnail_set_system()
 *         and menu_thumbnail_set_content*()
 * NOTE 2: 'playlist' and 'idx' are only required here for
 *         on-demand thumbnail download support
 *         (an annoyance...) */ 
void menu_thumbnail_request(
      menu_thumbnail_path_data_t *path_data, enum menu_thumbnail_id thumbnail_id,
      playlist_t *playlist, size_t idx, menu_thumbnail_t *thumbnail);

/* Resets (and free()s the current texture of) the
 * specified thumbnail */
void menu_thumbnail_reset(menu_thumbnail_t *thumbnail);

/* Stream processing */

/* Handles streaming of the specified thumbnail as it moves
 * on/off screen
 * - Must be called each frame for every on-screen entry
 * - Must be called once for each entry as it moves off-screen
 *   (or can be called each frame - overheads are small)
 * NOTE 1: Must be called *after* menu_thumbnail_set_system()
 * NOTE 2: This function calls menu_thumbnail_set_content*()
 * NOTE 3: This function is intended for use in situations
 *         where each menu entry has a *single* thumbnail.
 *         If each entry has two thumbnails, use
 *         menu_thumbnail_process_streams() for improved
 *         performance */
void menu_thumbnail_process_stream(
      menu_thumbnail_path_data_t *path_data, enum menu_thumbnail_id thumbnail_id,
      playlist_t *playlist, size_t idx, menu_thumbnail_t *thumbnail, bool on_screen);

/* Handles streaming of the specified thumbnails as they move
 * on/off screen
 * - Must be called each frame for every on-screen entry
 * - Must be called once for each entry as it moves off-screen
 *   (or can be called each frame - overheads are small)
 * NOTE 1: Must be called *after* menu_thumbnail_set_system()
 * NOTE 2: This function calls menu_thumbnail_set_content*()
 * NOTE 3: This function is intended for use in situations
 *         where each menu entry has *two* thumbnails.
 *         If each entry only has a single thumbnail, use
 *         menu_thumbnail_process_stream() for improved
 *         performance */
void menu_thumbnail_process_streams(
      menu_thumbnail_path_data_t *path_data,
      playlist_t *playlist, size_t idx,
      menu_thumbnail_t *right_thumbnail, menu_thumbnail_t *left_thumbnail,
      bool on_screen);

/* Thumbnail rendering */

/* Determines the actual screen dimensions of a
 * thumbnail when centred with aspect correct
 * scaling within a rectangle of (width x height) */
void menu_thumbnail_get_draw_dimensions(
      menu_thumbnail_t *thumbnail,
      unsigned width, unsigned height, float scale_factor,
      float *draw_width, float *draw_height);

/* Draws specified thumbnail centred (with aspect correct
 * scaling) within a rectangle of (width x height)
 * NOTE: Setting scale_factor > 1.0f will increase the
 *       size of the thumbnail beyond the limits of the
 *       (width x height) rectangle (centring + aspect
 *       correct scaling is preserved). Use with caution */
void menu_thumbnail_draw(
      video_frame_info_t *video_info, menu_thumbnail_t *thumbnail,
      float x, float y, unsigned width, unsigned height,
      float alpha, float scale_factor);

RETRO_END_DECLS

#endif

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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <features/features_cpu.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include <lists/file_list.h>

#include "gfx_display.h"
#include "gfx_animation.h"
#include "gfx_thumbnail.h"

#include "../configuration.h"
#include "../msg_hash.h"
#include "../paths.h"
#include "../file_path_special.h"

#include "../tasks/tasks_internal.h"

#define DEFAULT_GFX_THUMBNAIL_STREAM_DELAY  16.66667f * 3
#define DEFAULT_GFX_THUMBNAIL_FADE_DURATION 166.66667f

/* Utility structure, sent as userdata when pushing
 * an image load */
typedef struct
{
   uint64_t list_id;
   gfx_thumbnail_t *thumbnail;
} gfx_thumbnail_tag_t;

static gfx_thumbnail_state_t gfx_thumb_st = {0}; /* uint64_t alignment */

gfx_thumbnail_state_t *gfx_thumb_get_ptr(void)
{
   return &gfx_thumb_st;
}

/* Setters */

/* When streaming thumbnails, sets time in ms that an
 * entry must be on screen before an image load is
 * requested
 * > if 'delay' is negative, default value is set */
void gfx_thumbnail_set_stream_delay(float delay)
{
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;

   p_gfx_thumb->stream_delay = (delay >= 0.0f) ?
         delay : DEFAULT_GFX_THUMBNAIL_STREAM_DELAY;
}

/* Sets duration in ms of the thumbnail 'fade in'
 * animation
 * > If 'duration' is negative, default value is set */
void gfx_thumbnail_set_fade_duration(float duration)
{
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;

   p_gfx_thumb->fade_duration = (duration >= 0.0f) ?
         duration : DEFAULT_GFX_THUMBNAIL_FADE_DURATION;
}

/* Specifies whether 'fade in' animation should be
 * triggered for missing thumbnails
 * > When 'true', allows menu driver to animate
 *   any 'thumbnail unavailable' notifications */
void gfx_thumbnail_set_fade_missing(bool fade_missing)
{
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;

   p_gfx_thumb->fade_missing = fade_missing;
}

/* Callbacks */

/* Fade animation callback - simply resets thumbnail
 * 'fade_active' status */
static void gfx_thumbnail_fade_cb(void *userdata)
{
   gfx_thumbnail_t *thumbnail = (gfx_thumbnail_t*)userdata;
   if (thumbnail)
      thumbnail->flags |= GFX_THUMB_FLAG_FADE_ACTIVE;
}

/* Initialises thumbnail 'fade in' animation */
static void gfx_thumbnail_init_fade(
      gfx_thumbnail_state_t *p_gfx_thumb,
      gfx_thumbnail_t *thumbnail)
{
   /* Sanity check */
   if (!thumbnail)
      return;

   /* A 'fade in' animation is triggered if:
    * - Thumbnail is available
    * - Thumbnail is missing and 'fade_missing' is enabled */
   if (   (thumbnail->status == GFX_THUMBNAIL_STATUS_AVAILABLE)
       || (p_gfx_thumb->fade_missing
       && (thumbnail->status == GFX_THUMBNAIL_STATUS_MISSING)))
   {
      if (p_gfx_thumb->fade_duration > 0.0f)
      {
         gfx_animation_ctx_entry_t animation_entry;

         thumbnail->alpha                 = 0.0f;
         thumbnail->flags                |= GFX_THUMB_FLAG_FADE_ACTIVE;

         animation_entry.easing_enum      = EASING_OUT_QUAD;
         animation_entry.tag              = (uintptr_t)&thumbnail->alpha;
         animation_entry.duration         = p_gfx_thumb->fade_duration;
         animation_entry.target_value     = 1.0f;
         animation_entry.subject          = &thumbnail->alpha;
         animation_entry.cb               = gfx_thumbnail_fade_cb;
         animation_entry.userdata         = thumbnail;

         gfx_animation_push(&animation_entry);
      }
      else
         thumbnail->alpha = 1.0f;
   }
}

/* Used to process thumbnail data following completion
 * of image load task */
static void gfx_thumbnail_handle_upload(
      retro_task_t *task, void *task_data, void *user_data, const char *err)
{
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;
   struct texture_image *img          = (struct texture_image*)task_data;
   gfx_thumbnail_tag_t *thumbnail_tag = (gfx_thumbnail_tag_t*)user_data;
   bool fade_enabled                  = false;

   /* Sanity check */
   if (!thumbnail_tag)
      goto end;

   /* Ensure that we are operating on the correct
    * thumbnail... */
   if (thumbnail_tag->list_id != p_gfx_thumb->list_id)
      goto end;

   /* Only process image if we are waiting for it */
   if (thumbnail_tag->thumbnail->status != GFX_THUMBNAIL_STATUS_PENDING)
      goto end;

   /* Sanity check: if thumbnail already has a texture,
    * we're in some kind of weird error state - in this
    * case, the best course of action is to just reset
    * the thumbnail... */
   if (thumbnail_tag->thumbnail->texture)
      gfx_thumbnail_reset(thumbnail_tag->thumbnail);

   /* Set thumbnail 'missing' status by default
    * (saves a number of checks later) */
   thumbnail_tag->thumbnail->status = GFX_THUMBNAIL_STATUS_MISSING;

   /* If we reach this stage, thumbnail 'fade in'
    * animations should be applied (based on current
    * thumbnail status and global configuration) */
   fade_enabled = true;

   /* Check we have a valid image */
   if (!img || (img->width < 1) || (img->height < 1))
      goto end;

   /* Upload texture to GPU */
   if (!video_driver_texture_load(
            img, TEXTURE_FILTER_MIPMAP_LINEAR,
            &thumbnail_tag->thumbnail->texture))
      goto end;

   /* Cache dimensions */
   thumbnail_tag->thumbnail->width  = img->width;
   thumbnail_tag->thumbnail->height = img->height;

   /* Update thumbnail status */
   thumbnail_tag->thumbnail->status = GFX_THUMBNAIL_STATUS_AVAILABLE;

end:
   /* Clean up */
   if (img)
   {
      image_texture_free(img);
      free(img);
   }

   if (thumbnail_tag)
   {
      /* Trigger 'fade in' animation, if required */
      if (fade_enabled)
         gfx_thumbnail_init_fade(p_gfx_thumb,
               thumbnail_tag->thumbnail);

      free(thumbnail_tag);
   }
}

/* Core interface */

/* When called, prevents the handling of any pending
 * thumbnail load requests
 * >> **MUST** be called before deleting any gfx_thumbnail_t
 *    objects passed to gfx_thumbnail_request() or
 *    gfx_thumbnail_process_stream(), otherwise
 *    heap-use-after-free errors *will* occur */
void gfx_thumbnail_cancel_pending_requests(void)
{
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;

   p_gfx_thumb->list_id++;
}

/* Fetches the current thumbnail file path of the
 * specified thumbnail 'type'.
 * Returns true if path is valid. */
static bool gfx_thumbnail_get_path(
      gfx_thumbnail_path_data_t *path_data,
      enum gfx_thumbnail_id thumbnail_id,
      const char **path)
{
   if (path_data && path)
   {
      switch (thumbnail_id)
      {
         case GFX_THUMBNAIL_RIGHT:
            if (*path_data->right_path)
            {
               *path          = path_data->right_path;
               return true;
            }
            break;
         case GFX_THUMBNAIL_LEFT:
            if (*path_data->left_path)
            {
               *path          = path_data->left_path;
               return true;
            }
         case GFX_THUMBNAIL_ICON:
            if (*path_data->icon_path)
            {
               *path          = path_data->icon_path;
               return true;
            }
            break;
         default:
            break;
      }
   }

   return false;
}


/* Requests loading of the specified thumbnail
 * - If operation fails, 'thumbnail->status' will be set to
 *   GFX_THUMBNAIL_STATUS_MISSING
 * - If operation is successful, 'thumbnail->status' will be
 *   set to GFX_THUMBNAIL_STATUS_PENDING
 * 'thumbnail' will be populated with texture info/metadata
 * once the image load is complete
 * NOTE 1: Must be called *after* gfx_thumbnail_set_system()
 *         and gfx_thumbnail_set_content*()
 * NOTE 2: 'playlist' and 'idx' are only required here for
 *         on-demand thumbnail download support
 *         (an annoyance...) */
void gfx_thumbnail_request(
      gfx_thumbnail_path_data_t *path_data,
      enum gfx_thumbnail_id thumbnail_id,
      playlist_t *playlist,
      size_t idx,
      gfx_thumbnail_t *thumbnail,
      unsigned gfx_thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails)
{
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;

   if (!path_data || !thumbnail)
      return;

   /* Reset thumbnail, then set 'missing' status by default
    * (saves a number of checks later) */
   gfx_thumbnail_reset(thumbnail);
   thumbnail->status = GFX_THUMBNAIL_STATUS_MISSING;

   /* Update/extract thumbnail path */
   if (gfx_thumbnail_is_enabled(path_data, thumbnail_id))
   {
      if (gfx_thumbnail_update_path(path_data, thumbnail_id))
      {
         const char *thumbnail_path = NULL;
         if (gfx_thumbnail_get_path(path_data, thumbnail_id, &thumbnail_path))
         {
            /* Load thumbnail, if required */
            if (path_is_valid(thumbnail_path))
            {
               gfx_thumbnail_tag_t *thumbnail_tag =
                  (gfx_thumbnail_tag_t*)malloc(sizeof(gfx_thumbnail_tag_t));

               if (!thumbnail_tag)
                  goto end;

               /* Configure user data */
               thumbnail_tag->thumbnail = thumbnail;
               thumbnail_tag->list_id   = p_gfx_thumb->list_id;

               /* Would like to cancel any existing image load tasks
                * here, but can't see how to do it... */
               if (task_push_image_load(
                        thumbnail_path, video_driver_supports_rgba(),
                        gfx_thumbnail_upscale_threshold,
                        gfx_thumbnail_handle_upload, thumbnail_tag))
                  thumbnail->status = GFX_THUMBNAIL_STATUS_PENDING;
            }
#ifdef HAVE_NETWORKING
            /* Handle on demand thumbnail downloads */
            else if (network_on_demand_thumbnails)
            {
               enum playlist_thumbnail_name_flags curr_flag;
               static char last_img_name[PATH_MAX_LENGTH] = {0};
               bool playlist_use_filename                 = config_get_ptr()->bools.playlist_use_filename;
               if (!playlist)
                  goto end;
               /* Only trigger a thumbnail download if image
                * name has changed since the last call of
                * gfx_thumbnail_request()
                * > Allows gfx_thumbnail_request() to be used
                *   for successive right/left thumbnail requests
                *   with minimal duplication of effort
                *   (i.e. task_push_pl_entry_thumbnail_download()
                *   will automatically cancel if a download for the
                *   existing playlist entry is pending, but the
                *   checks required for this involve significant
                *   overheads. We can avoid this entirely with
                *   a simple string comparison) */
               if (*path_data->content_img)
                  if (string_is_equal(path_data->content_img, last_img_name))
                     goto end;

               strlcpy(last_img_name, path_data->content_img, sizeof(last_img_name));

               /* Get system name */
               if (!*path_data->system)
                  goto end;

               /* Since task_push_pl_entry_download will shift the flag, do not attempt if it is already
                * at second to last option. */
               curr_flag = playlist_get_curr_thumbnail_name_flag(playlist,idx);
               if (   curr_flag & PLAYLIST_THUMBNAIL_FLAG_NONE
                   || curr_flag & PLAYLIST_THUMBNAIL_FLAG_SHORT_NAME)
                  goto end;
               /* Do not try to fetch full names here, if it is not explicitly wanted */
               if (   !playlist_use_filename
                   && !playlist_thumbnail_match_with_filename(playlist)
                   && curr_flag == PLAYLIST_THUMBNAIL_FLAG_INVALID)
                    playlist_update_thumbnail_name_flag(playlist, idx, PLAYLIST_THUMBNAIL_FLAG_FULL_NAME);

               /* Trigger thumbnail download *
                * Note: download will grab all 3 possible thumbnails, no matter
                * what left/right thumbnails are set at the moment */
               task_push_pl_entry_thumbnail_download(path_data->system, playlist,
                     (unsigned)idx, false, true);
            }
#endif
         }
      }
   }

end:
   /* Trigger 'fade in' animation, if required */
   if (thumbnail->status != GFX_THUMBNAIL_STATUS_PENDING)
      gfx_thumbnail_init_fade(p_gfx_thumb,
            thumbnail);
}

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
      unsigned gfx_thumbnail_upscale_threshold)
{
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;
   gfx_thumbnail_tag_t *thumbnail_tag = NULL;

   if (!thumbnail)
      return;

   /* Reset thumbnail, then set 'missing' status by default
    * (saves a number of checks later) */
   gfx_thumbnail_reset(thumbnail);
   thumbnail->status = GFX_THUMBNAIL_STATUS_MISSING;

   /* Check if file path is valid */
   if (   (!file_path || !*file_path)
       || !path_is_valid(file_path))
      return;

   /* Load thumbnail */
   if (!(thumbnail_tag = (gfx_thumbnail_tag_t*)malloc(sizeof(gfx_thumbnail_tag_t))))
      return;

   /* Configure user data */
   thumbnail_tag->thumbnail = thumbnail;
   thumbnail_tag->list_id   = p_gfx_thumb->list_id;

   /* Would like to cancel any existing image load tasks
    * here, but can't see how to do it... */
   if (task_push_image_load(
         file_path, video_driver_supports_rgba(),
         gfx_thumbnail_upscale_threshold,
         gfx_thumbnail_handle_upload, thumbnail_tag))
      thumbnail->status = GFX_THUMBNAIL_STATUS_PENDING;
}

/* Resets (and free()s the current texture of) the
 * specified thumbnail */
void gfx_thumbnail_reset(gfx_thumbnail_t *thumbnail)
{
   if (!thumbnail)
      return;

   /* Unload texture */
   if (thumbnail->texture)
      video_driver_texture_unload(&thumbnail->texture);

   /* Ensure any 'fade in' animation is killed */
   if (thumbnail->flags & GFX_THUMB_FLAG_FADE_ACTIVE)
   {
      uintptr_t tag = (uintptr_t)&thumbnail->alpha;
      gfx_animation_kill_by_tag(&tag);
   }

   /* Reset all parameters */
   thumbnail->status      = GFX_THUMBNAIL_STATUS_UNKNOWN;
   thumbnail->texture     = 0;
   thumbnail->width       = 0;
   thumbnail->height      = 0;
   thumbnail->alpha       = 0.0f;
   thumbnail->delay_timer = 0.0f;
   thumbnail->flags      &= ~(GFX_THUMB_FLAG_FADE_ACTIVE
                            | GFX_THUMB_FLAG_CORE_ASPECT);
}

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
      bool network_on_demand_thumbnails)
{
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;

   /* Only process request if current status
    * is GFX_THUMBNAIL_STATUS_UNKNOWN */
   if (   !thumbnail
       || (thumbnail->status != GFX_THUMBNAIL_STATUS_UNKNOWN))
      return;

   /* Check if stream delay timer has elapsed */
   thumbnail->delay_timer += p_anim->delta_time;

   if (thumbnail->delay_timer > p_gfx_thumb->stream_delay)
   {
      /* Sanity check */
      if (!path_data)
      {
         /* No path information
          * > Reset thumbnail and set missing status
          *   to prevent repeated load attempts */
         gfx_thumbnail_reset(thumbnail);
         thumbnail->status = GFX_THUMBNAIL_STATUS_MISSING;
         thumbnail->alpha  = 1.0f;
         return;
      }

      /* Request image load */
      gfx_thumbnail_request(
            path_data, thumbnail_id, playlist, idx, thumbnail,
            gfx_thumbnail_upscale_threshold,
            network_on_demand_thumbnails);
   }
}

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
      bool network_on_demand_thumbnails)
{
   bool process_r = false;
   bool process_l = false;

   if (!right_thumbnail || !left_thumbnail)
      return;

   /* Only process request if current status
    * is GFX_THUMBNAIL_STATUS_UNKNOWN */
   process_r = (right_thumbnail->status == GFX_THUMBNAIL_STATUS_UNKNOWN);
   process_l = (left_thumbnail->status  == GFX_THUMBNAIL_STATUS_UNKNOWN);

   if (process_r || process_l)
   {
      /* Check if stream delay timer has elapsed */
      gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;
      float delta_time                   = p_anim->delta_time;
      bool request_r                     = false;
      bool request_l                     = false;

      if (process_r)
      {
         right_thumbnail->delay_timer += delta_time;
         request_r                     =
               (right_thumbnail->delay_timer > p_gfx_thumb->stream_delay);
      }

      if (process_l)
      {
         left_thumbnail->delay_timer  += delta_time;
         request_l                     =
               (left_thumbnail->delay_timer > p_gfx_thumb->stream_delay);
      }

      /* Check if one or more thumbnails should be requested */
      if (request_r || request_l)
      {
         /* Sanity check */
         if (!path_data)
         {
            /* No path information
             * > Reset thumbnail and set missing status
             *   to prevent repeated load attempts */
            if (request_r)
            {
               gfx_thumbnail_reset(right_thumbnail);
               right_thumbnail->status = GFX_THUMBNAIL_STATUS_MISSING;
               right_thumbnail->alpha  = 1.0f;
            }

            if (request_l)
            {
               gfx_thumbnail_reset(left_thumbnail);
               left_thumbnail->status  = GFX_THUMBNAIL_STATUS_MISSING;
               left_thumbnail->alpha   = 1.0f;
            }

            return;
         }

         /* Request image load */
         if (request_r)
            gfx_thumbnail_request(
                  path_data, GFX_THUMBNAIL_RIGHT, playlist, idx, right_thumbnail,
                  gfx_thumbnail_upscale_threshold,
                  network_on_demand_thumbnails);

         if (request_l)
            gfx_thumbnail_request(
                  path_data, GFX_THUMBNAIL_LEFT, playlist, idx, left_thumbnail,
                  gfx_thumbnail_upscale_threshold,
                  network_on_demand_thumbnails);
      }
   }
}

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
      bool network_on_demand_thumbnails)
{
   if (!thumbnail)
      return;

   if (on_screen)
   {
      /* Entry is on-screen
       * > Only process if current status is
       *   GFX_THUMBNAIL_STATUS_UNKNOWN */
      if (thumbnail->status == GFX_THUMBNAIL_STATUS_UNKNOWN)
      {
         gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;

         /* Check if stream delay timer has elapsed */
         thumbnail->delay_timer += p_anim->delta_time;

         if (thumbnail->delay_timer > p_gfx_thumb->stream_delay)
         {
            /* Update thumbnail content */
            if (   !path_data
                || !playlist
                || !gfx_thumbnail_set_content_playlist(path_data, playlist, idx))
            {
               /* Content is invalid
                * > Reset thumbnail and set missing status */
               gfx_thumbnail_reset(thumbnail);
               thumbnail->status = GFX_THUMBNAIL_STATUS_MISSING;
               thumbnail->alpha  = 1.0f;
               return;
            }

            /* Request image load */
            gfx_thumbnail_request(
                  path_data, thumbnail_id, playlist, idx, thumbnail,
                  gfx_thumbnail_upscale_threshold,
                  network_on_demand_thumbnails);
         }
      }
   }
   else
   {
      /* Entry is off-screen
       * > If status is GFX_THUMBNAIL_STATUS_UNKNOWN,
       *   thumbnail is already in a blank state - but we
       *   must ensure that delay timer is set to zero */
      if (thumbnail->status == GFX_THUMBNAIL_STATUS_UNKNOWN)
         thumbnail->delay_timer = 0.0f;
      /* In all other cases, reset thumbnail */
      else
         gfx_thumbnail_reset(thumbnail);
   }
}

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
      bool network_on_demand_thumbnails)
{
   if (!right_thumbnail || !left_thumbnail)
      return;

   if (on_screen)
   {
      /* Entry is on-screen
       * > Only process if current status is
       *   GFX_THUMBNAIL_STATUS_UNKNOWN */
      bool process_r = (right_thumbnail->status == GFX_THUMBNAIL_STATUS_UNKNOWN);
      bool process_l = (left_thumbnail->status  == GFX_THUMBNAIL_STATUS_UNKNOWN);

      if (process_r || process_l)
      {
         /* Check if stream delay timer has elapsed */
         gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;
         float delta_time                   = p_anim->delta_time;
         bool request_r                     = false;
         bool request_l                     = false;

         if (process_r)
         {
            right_thumbnail->delay_timer += delta_time;
            request_r                     =
                  (right_thumbnail->delay_timer > p_gfx_thumb->stream_delay);
         }

         if (process_l)
         {
            left_thumbnail->delay_timer  += delta_time;
            request_l                     =
                  (left_thumbnail->delay_timer > p_gfx_thumb->stream_delay);
         }

         /* Check if one or more thumbnails should be requested */
         if (request_r || request_l)
         {
            /* Update thumbnail content */
            if (   !path_data
                || !playlist
                || !gfx_thumbnail_set_content_playlist(path_data, playlist, idx))
            {
               /* Content is invalid
                * > Reset thumbnail and set missing status */
               if (request_r)
               {
                  gfx_thumbnail_reset(right_thumbnail);
                  right_thumbnail->status = GFX_THUMBNAIL_STATUS_MISSING;
                  right_thumbnail->alpha  = 1.0f;
               }

               if (request_l)
               {
                  gfx_thumbnail_reset(left_thumbnail);
                  left_thumbnail->status  = GFX_THUMBNAIL_STATUS_MISSING;
                  left_thumbnail->alpha   = 1.0f;
               }

               return;
            }

            /* Request image load */
            if (request_r)
               gfx_thumbnail_request(
                     path_data, GFX_THUMBNAIL_RIGHT, playlist, idx, right_thumbnail,
                     gfx_thumbnail_upscale_threshold,
                     network_on_demand_thumbnails);

            if (request_l)
               gfx_thumbnail_request(
                     path_data, GFX_THUMBNAIL_LEFT, playlist, idx, left_thumbnail,
                     gfx_thumbnail_upscale_threshold,
                     network_on_demand_thumbnails);
         }
      }
   }
   else
   {
      /* Entry is off-screen
       * > If status is GFX_THUMBNAIL_STATUS_UNKNOWN,
       *   thumbnail is already in a blank state - but we
       *   must ensure that delay timer is set to zero
       * > In all other cases, reset thumbnail */
      if (right_thumbnail->status == GFX_THUMBNAIL_STATUS_UNKNOWN)
         right_thumbnail->delay_timer = 0.0f;
      else
         gfx_thumbnail_reset(right_thumbnail);

      if (left_thumbnail->status == GFX_THUMBNAIL_STATUS_UNKNOWN)
         left_thumbnail->delay_timer = 0.0f;
      else
         gfx_thumbnail_reset(left_thumbnail);
   }
}

/* Thumbnail rendering */

/* Determines the actual screen dimensions of a
 * thumbnail when centred with aspect correct
 * scaling within a rectangle of (width x height) */
void gfx_thumbnail_get_draw_dimensions(
      gfx_thumbnail_t *thumbnail,
      unsigned width, unsigned height, float scale_factor,
      float *draw_width, float *draw_height)
{
   float core_aspect;
   float display_aspect;
   float thumbnail_aspect;
   video_driver_state_t *video_st = video_state_get_ptr();

   /* Sanity check */
   if (   !thumbnail
       || (width             < 1)
       || (height            < 1)
       || (thumbnail->width  < 1)
       || (thumbnail->height < 1))
   {
      *draw_width  = 0.0f;
      *draw_height = 0.0f;
      return;
   }

   /* Account for display/thumbnail/core aspect ratio
    * differences */
   display_aspect   = (float)width            / (float)height;
   thumbnail_aspect = (float)thumbnail->width / (float)thumbnail->height;
   core_aspect      = ((thumbnail->flags & GFX_THUMB_FLAG_CORE_ASPECT)
         && video_st && video_st->av_info.geometry.aspect_ratio > 0)
               ? video_st->av_info.geometry.aspect_ratio
               : thumbnail_aspect;

   if (thumbnail_aspect > display_aspect)
   {
      *draw_width  = (float)width;
      *draw_height = (float)thumbnail->height * (*draw_width / (float)thumbnail->width);

      if (thumbnail->flags & GFX_THUMB_FLAG_CORE_ASPECT)
      {
         *draw_height = *draw_height * (thumbnail_aspect / core_aspect);

         if (*draw_height > height)
         {
            *draw_height = (float)height;
            *draw_width  = (float)thumbnail->width * (*draw_height / (float)thumbnail->height);
            *draw_width  = *draw_width / (thumbnail_aspect / core_aspect);
         }
      }
   }
   else
   {
      *draw_height = (float)height;
      *draw_width  = (float)thumbnail->width * (*draw_height / (float)thumbnail->height);

      if (thumbnail->flags & GFX_THUMB_FLAG_CORE_ASPECT)
         *draw_width  = *draw_width / (thumbnail_aspect / core_aspect);
   }

   /* Final overwidth check */
   if (*draw_width > width)
   {
      *draw_width  = (float)width;
      *draw_height = (float)thumbnail->height * (*draw_width / (float)thumbnail->width);

      if (thumbnail->flags & GFX_THUMB_FLAG_CORE_ASPECT)
         *draw_height = *draw_height * (thumbnail_aspect / core_aspect);
   }

   /* Account for scale factor
    * > Side note: We cannot use the gfx_display_ctx_draw_t
    *   'scale_factor' parameter for scaling thumbnails,
    *   since this clips off any part of the expanded image
    *   that extends beyond the bounding box. But even if
    *   it didn't, we can't get real screen dimensions
    *   without scaling manually... */
   *draw_width  *= scale_factor;
   *draw_height *= scale_factor;
}

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
      gfx_thumbnail_shadow_t *shadow)
{
   gfx_display_t            *p_disp  = disp_get_ptr();
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;
   /* Sanity check */
   if (
            !thumbnail
         || !dispctx
         || (width         < 1)
         || (height        < 1)
         || (alpha        <= 0.0f)
         || (scale_factor <= 0.0f)
      )
      return;

   /* Only draw thumbnail if it is available... */
   if (thumbnail->status == GFX_THUMBNAIL_STATUS_AVAILABLE)
   {
      gfx_display_ctx_draw_t draw;
      struct video_coords coords;
      math_matrix_4x4 mymat;
      float draw_width;
      float draw_height;
      float draw_x;
      float draw_y;
      float thumbnail_alpha     = thumbnail->alpha * alpha;
      float thumbnail_color[16] = {
         1.0f, 1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, 1.0f
      };

      /* Set thumbnail opacity */
      if (thumbnail_alpha <= 0.0f)
         return;
      if (thumbnail_alpha < 1.0f)
         gfx_display_set_alpha(thumbnail_color, thumbnail_alpha);

      /* Get thumbnail dimensions */
      gfx_thumbnail_get_draw_dimensions(
            thumbnail, width, height, scale_factor,
            &draw_width, &draw_height);

      if (dispctx->blend_begin)
         dispctx->blend_begin(userdata);

      if (!p_disp->dispctx->handles_transform)
      {
         /* Perform 'rotation' step
          * > Note that rotation does not actually work...
          * > It rotates the image all right, but distorts it
          *   to fit the aspect of the bounding box while clipping
          *   off any 'corners' that extend beyond the bounding box
          * > Since the result is visual garbage, we disable
          *   rotation entirely
          * > But we still have to call gfx_display_rotate_z(),
          *   or nothing will be drawn...
          */
         float cosine             = 1.0f; /* cos(rad)  = cos(0)  = 1.0f */
         float sine               = 0.0f; /* sine(rad) = sine(0) = 0.0f */
         gfx_display_rotate_z(p_disp, &mymat, cosine, sine, userdata);
      }

      /* Configure draw object
       * > Note: Colour, width/height and position must
       *   be set *after* drawing any shadow effects */
      coords.vertices      = 4;
      coords.vertex        = NULL;
      coords.tex_coord     = NULL;
      coords.lut_tex_coord = NULL;

      draw.scale_factor    = 1.0f;
      draw.rotation        = 0.0f;
      draw.coords          = &coords;
      draw.matrix_data     = &mymat;
      draw.texture         = thumbnail->texture;
      draw.prim_type       = GFX_DISPLAY_PRIM_TRIANGLESTRIP;
      draw.pipeline_id     = 0;

      /* Set thumbnail alignment within bounding box */
      switch (alignment)
      {
         case GFX_THUMBNAIL_ALIGN_TOP:
            /* Centred horizontally */
            draw_x = x + ((float)width - draw_width) / 2.0f;
            /* Drawn at top of bounding box */
            draw_y = (float)video_height - y - draw_height;
            break;
         case GFX_THUMBNAIL_ALIGN_BOTTOM:
            /* Centred horizontally */
            draw_x = x + ((float)width - draw_width) / 2.0f;
            /* Drawn at bottom of bounding box */
            draw_y = (float)video_height - y - (float)height;
            break;
         case GFX_THUMBNAIL_ALIGN_LEFT:
            /* Drawn at left side of bounding box */
            draw_x = x;
            /* Centred vertically */
            draw_y = (float)video_height - y - draw_height - ((float)height - draw_height) / 2.0f;
            break;
         case GFX_THUMBNAIL_ALIGN_RIGHT:
            /* Drawn at right side of bounding box */
            draw_x = x + (float)width - draw_width;
            /* Centred vertically */
            draw_y = (float)video_height - y - draw_height - ((float)height - draw_height) / 2.0f;
            break;
         case GFX_THUMBNAIL_ALIGN_CENTRE:
         default:
            /* Centred both horizontally and vertically */
            draw_x = x + ((float)width - draw_width) / 2.0f;
            draw_y = (float)video_height - y - draw_height - ((float)height - draw_height) / 2.0f;
            break;
      }

      /* Draw shadow effect, if required */
      if (shadow)
      {
         /* Sanity check */
         if (     (shadow->type != GFX_THUMBNAIL_SHADOW_NONE)
               && (shadow->alpha > 0.0f))
         {
            float shadow_width;
            float shadow_height;
            float shadow_x;
            float shadow_y;
            float shadow_color[16] = {
               0.0f, 0.0f, 0.0f, 1.0f,
               0.0f, 0.0f, 0.0f, 1.0f,
               0.0f, 0.0f, 0.0f, 1.0f,
               0.0f, 0.0f, 0.0f, 1.0f
            };
            float shadow_alpha     = thumbnail_alpha;

            /* Set shadow opacity */
            if (shadow->alpha < 1.0f)
               shadow_alpha *= shadow->alpha;

            gfx_display_set_alpha(shadow_color, shadow_alpha);

            /* Configure shadow based on effect type
             * > Not using a switch() here, since we've
             *   already eliminated GFX_THUMBNAIL_SHADOW_NONE */
            if (shadow->type == GFX_THUMBNAIL_SHADOW_OUTLINE)
            {
               shadow_width  = draw_width  + (float)(shadow->outline.width * 2);
               shadow_height = draw_height + (float)(shadow->outline.width * 2);
               shadow_x      = draw_x - (float)shadow->outline.width;
               shadow_y      = draw_y - (float)shadow->outline.width;
            }
            /* Default: GFX_THUMBNAIL_SHADOW_DROP */
            else
            {
               shadow_width  = draw_width;
               shadow_height = draw_height;
               shadow_x      = draw_x + shadow->drop.x_offset;
               shadow_y      = draw_y - shadow->drop.y_offset;
            }

            /* Apply shadow draw object configuration */
            coords.color = (const float*)shadow_color;
            draw.width   = (unsigned)shadow_width;
            draw.height  = (unsigned)shadow_height;
            draw.x       = shadow_x;
            draw.y       = shadow_y;

            /* Draw shadow */
            if (draw.height > 0 && draw.width > 0)
               if (dispctx->draw)
                  dispctx->draw(&draw, userdata, video_width, video_height);
         }
      }

      /* Final thumbnail draw object configuration */
      coords.color = (const float*)thumbnail_color;
      draw.width   = (unsigned)draw_width;
      draw.height  = (unsigned)draw_height;
      draw.x       = draw_x;
      draw.y       = draw_y;

      /* Draw thumbnail */
      if (draw.height > 0 && draw.width > 0)
         if (dispctx->draw)
            dispctx->draw(&draw, userdata, video_width, video_height);

      if (dispctx->blend_end)
         dispctx->blend_end(userdata);
   }
}

/* Returns currently set thumbnail 'type' (Named_Snaps,
 * Named_Titles, Named_Boxarts, Named_Logos) for specified thumbnail
 * identifier (right, left) */
static const char *gfx_thumbnail_get_type(
      unsigned gfx_thumbnails,
      unsigned left_thumbnails,
      unsigned icon_thumbnails,
      gfx_thumbnail_path_data_t *path_data,
      enum gfx_thumbnail_id thumbnail_id)
{
   if (path_data)
   {
      unsigned type                 = 0;
      switch (thumbnail_id)
      {
         case GFX_THUMBNAIL_RIGHT:
            if (   path_data->playlist_right_mode 
                != PLAYLIST_THUMBNAIL_MODE_DEFAULT)
               type = (unsigned)path_data->playlist_right_mode - 1;
            else
               type = gfx_thumbnails;
            break;
         case GFX_THUMBNAIL_LEFT:
            if (   path_data->playlist_left_mode 
                != PLAYLIST_THUMBNAIL_MODE_DEFAULT)
               type = (unsigned)path_data->playlist_left_mode - 1;
            else
               type = left_thumbnails;
            break;
         case GFX_THUMBNAIL_ICON:
            if (   path_data->playlist_icon_mode 
                != PLAYLIST_THUMBNAIL_MODE_DEFAULT)
               type = (unsigned)path_data->playlist_icon_mode - 1;
            else
               type = icon_thumbnails;
            break;
         default:
            goto end;
      }

      switch (type)
      {
         case 1:
            return "Named_Snaps";
         case 2:
            return "Named_Titles";
         case 3:
            return "Named_Boxarts";
         case 4:
            return "Named_Logos";
         case 0:
         default:
            break;
      }
   }

end:
   return msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF);
}

/* Fills content_img field of path_data using existing
 * content_label field (for internal use only) */
void gfx_thumbnail_fill_content_img(char *s,
   size_t len, const char *src, bool shorten)
{
   char *scrub_char_ptr = NULL;
   /* Copy source label string */
   size_t _len          = strlcpy(s, src, len);

   /* Shortening logic: up to first space + bracket */
   if (shorten)
   {
      int bracketpos    = -1;
      if ((bracketpos = string_find_index_substring_string(src, " (")) > 0)
         _len = bracketpos;
      /* Explicit zero if short name is same as standard name - saves some queries later. */
      else
      {
         s[0] = '\0';
         return;
      }
   }
   /* Scrub characters that are not cross-platform and/or violate the
    * No-Intro filename standard:
    * https://datomatic.no-intro.org/stuff/The%20Official%20No-Intro%20Convention%20(20071030).pdf
    * Replace these characters in the entry name with underscores */
   while ((scrub_char_ptr = strpbrk(s, "&*/:`\"<>?\\|")))
      *scrub_char_ptr = '_';
   /* Add PNG extension */
   strlcpy(s + _len, ".png", len - _len);
}

/* Resets thumbnail path data
 * (blanks all internal string containers) */
void gfx_thumbnail_path_reset(gfx_thumbnail_path_data_t *path_data)
{
   if (!path_data)
      return;

   path_data->system_len           = 0;
   path_data->system[0]            = '\0';
   path_data->content_path[0]      = '\0';
   path_data->content_label_len    = 0;
   path_data->content_label[0]     = '\0';
   path_data->content_core_name[0] = '\0';
   path_data->content_db_name[0]   = '\0';
   path_data->content_img[0]       = '\0';
   path_data->content_img_full[0]  = '\0';
   path_data->content_img_short[0] = '\0';
   path_data->right_path[0]        = '\0';
   path_data->left_path[0]         = '\0';
   path_data->icon_path[0]         = '\0';

   path_data->playlist_right_mode = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   path_data->playlist_left_mode  = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   path_data->playlist_icon_mode  = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
}

/* Initialisation */

/* Creates new thumbnail path data container.
 * Returns handle to new gfx_thumbnail_path_data_t object.
 * on success, otherwise NULL.
 * Note: Returned object must be free()d */
gfx_thumbnail_path_data_t *gfx_thumbnail_path_init(void)
{
   gfx_thumbnail_path_data_t *path_data = (gfx_thumbnail_path_data_t*)
      malloc(sizeof(*path_data));
   if (!path_data)
      return NULL;

   gfx_thumbnail_path_reset(path_data);

   return path_data;
}

/* Utility Functions */

/* Returns true if specified thumbnail is enabled
 * (i.e. if 'type' is not equal to MENU_ENUM_LABEL_VALUE_OFF) */
bool gfx_thumbnail_is_enabled(gfx_thumbnail_path_data_t *path_data,
      enum gfx_thumbnail_id thumbnail_id)
{
   if (path_data)
   {
      settings_t          *settings = config_get_ptr();
      unsigned gfx_thumbnails       = settings->uints.gfx_thumbnails;
      unsigned menu_left_thumbnails = settings->uints.menu_left_thumbnails;
      unsigned menu_icon_thumbnails = settings->uints.menu_icon_thumbnails;

      switch (thumbnail_id)
      {
         case GFX_THUMBNAIL_RIGHT:
            if (   path_data->playlist_right_mode 
                != PLAYLIST_THUMBNAIL_MODE_DEFAULT)
               return path_data->playlist_right_mode != PLAYLIST_THUMBNAIL_MODE_OFF;
            return gfx_thumbnails != 0;
         case GFX_THUMBNAIL_LEFT:
            if (   path_data->playlist_left_mode 
                != PLAYLIST_THUMBNAIL_MODE_DEFAULT)
               return path_data->playlist_left_mode != PLAYLIST_THUMBNAIL_MODE_OFF;
            return menu_left_thumbnails != 0;
         case GFX_THUMBNAIL_ICON:
            if (   path_data->playlist_icon_mode 
                != PLAYLIST_THUMBNAIL_MODE_DEFAULT)
                return path_data->playlist_icon_mode != PLAYLIST_THUMBNAIL_MODE_OFF;
            return menu_icon_thumbnails != 0;
         default:
            break;
      }
   }

   return false;
}

/* Setters */

/* Sets current 'system' (default database name).
 * Returns true if 'system' is valid.
 * If playlist is provided, extracts system-specific
 * thumbnail assignment metadata (required for accurate
 * usage of gfx_thumbnail_is_enabled())
 * > Used as a fallback when individual content lacks an
 *   associated database name */
bool gfx_thumbnail_set_system(gfx_thumbnail_path_data_t *path_data,
      const char *system, playlist_t *playlist)
{
   if (!path_data)
      return false;

   /* When system is updated, must regenerate right/left
    * thumbnail paths */
   path_data->right_path[0]       = '\0';
   path_data->left_path[0]        = '\0';

   /* 'Reset' path_data system string */
   path_data->system_len          = 0;
   path_data->system[0]           = '\0';

   /* Must also reset playlist thumbnail display modes */
   path_data->playlist_right_mode = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   path_data->playlist_left_mode  = PLAYLIST_THUMBNAIL_MODE_DEFAULT;

   if (!system || !*system)
      return false;

   /* Hack: There is only one MAME thumbnail repo,
    * so filter any input starting with 'MAME...' */
   if (strncmp(system, "MAME", 4) == 0)
      path_data->system_len = strlcpy(path_data->system, "MAME", sizeof(path_data->system));
   else
      path_data->system_len = strlcpy(path_data->system, system, sizeof(path_data->system));

   /* Addendum: Now that we have per-playlist thumbnail display
    * modes, we must extract them here - otherwise
    * gfx_thumbnail_is_enabled() will go out of sync */
   if (playlist)
   {
      const char *playlist_path    = playlist_get_conf_path(playlist);

      /* Note: This is not considered an error
       * (just means that input playlist is ignored) */
      if (playlist_path && *playlist_path)
      {
         const char *playlist_file = path_basename_nocompression(playlist_path);
         /* Note: This is not considered an error
          * (just means that input playlist is ignored) */
         if (playlist_file && *playlist_file)
         {
            /* Check for history/favourites playlists */
            bool playlist_valid =
               (   memcmp(system, "history", 8) == 0
                && memcmp(playlist_file,
                   FILE_PATH_CONTENT_HISTORY,
                   sizeof(FILE_PATH_CONTENT_HISTORY)) == 0)
               || (     memcmp(system, "favorites", 10) == 0
                     && memcmp(playlist_file,
                        FILE_PATH_CONTENT_FAVORITES,
                        sizeof(FILE_PATH_CONTENT_FAVORITES)) == 0);

            /* This means we have to work a little harder
             * i.e. check whether the cached playlist file
             * matches the database name */
            if (!playlist_valid)
            {
               char playlist_name[NAME_MAX_LENGTH];
               fill_pathname(playlist_name, playlist_file, "", sizeof(playlist_name));
               playlist_valid = string_is_equal(playlist_name, system);
            }

            /* If we have a valid playlist, extract thumbnail modes */
            if (playlist_valid)
            {
               path_data->playlist_right_mode =
                  playlist_get_thumbnail_mode(playlist, PLAYLIST_THUMBNAIL_RIGHT);
               path_data->playlist_left_mode =
                  playlist_get_thumbnail_mode(playlist, PLAYLIST_THUMBNAIL_LEFT);
            }
         }
      }
   }

   return true;
}

/* Sets current thumbnail content according to the specified label.
 * Returns true if content is valid */
bool gfx_thumbnail_set_content(gfx_thumbnail_path_data_t *path_data, const char *label)
{
   if (!path_data)
      return false;

   /* When content is updated, must regenerate right/left
    * thumbnail paths */
   path_data->right_path[0]        = '\0';
   path_data->left_path[0]         = '\0';

   /* 'Reset' path_data content strings */
   path_data->content_path[0]      = '\0';
   path_data->content_label_len    = 0;
   path_data->content_label[0]     = '\0';
   path_data->content_core_name[0] = '\0';
   path_data->content_db_name[0]   = '\0';
   path_data->content_img[0]       = '\0';
   path_data->content_img_full[0]  = '\0';
   path_data->content_img_short[0] = '\0';

   /* Must also reset playlist thumbnail display modes */
   path_data->playlist_right_mode  = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   path_data->playlist_left_mode   = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   path_data->playlist_index       = 0;

   if (!label || !*label)
      return false;

   /* Cache content label */
   path_data->content_label_len = strlcpy(path_data->content_label,
         label, sizeof(path_data->content_label));

   /* Determine content image name */
   gfx_thumbnail_fill_content_img(path_data->content_img,
         sizeof(path_data->content_img),
         path_data->content_label, false);
   gfx_thumbnail_fill_content_img(path_data->content_img_short,
         sizeof(path_data->content_img_short),
         path_data->content_label, true);

   /* Have to set content path to *something*...
    * Just use label value (it doesn't matter) */
   strlcpy(path_data->content_path, label, sizeof(path_data->content_path));

   /* Redundant error check... */
   return *path_data->content_img;
}

/* Sets current thumbnail content to the specified image.
 * Returns true if content is valid */
bool gfx_thumbnail_set_content_image(
      gfx_thumbnail_path_data_t *path_data,
      const char *img_dir, const char *img_name)
{
   if (!path_data)
      return false;

   /* When content is updated, must regenerate right/left
    * thumbnail paths */
   path_data->right_path[0]        = '\0';
   path_data->left_path[0]         = '\0';

   /* 'Reset' path_data content strings */
   path_data->content_path[0]      = '\0';
   path_data->content_label_len    = 0;
   path_data->content_label[0]     = '\0';
   path_data->content_core_name[0] = '\0';
   path_data->content_db_name[0]   = '\0';
   path_data->content_img[0]       = '\0';
   path_data->content_img_full[0]  = '\0';
   path_data->content_img_short[0] = '\0';

   /* Must also reset playlist thumbnail display modes */
   path_data->playlist_right_mode  = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   path_data->playlist_left_mode   = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   path_data->playlist_index       = 0;

   if ((!img_dir || !*img_dir) || (!img_name || !*img_name))
      return false;

   if (path_is_media_type(img_name) != RARCH_CONTENT_IMAGE)
      return false;

   /* Cache content image name */
   strlcpy(path_data->content_img,
            img_name, sizeof(path_data->content_img));

   path_data->content_label_len = fill_pathname(
         path_data->content_label,
         path_data->content_img, "",
         sizeof(path_data->content_label));

   /* Set file path */
   fill_pathname_join_special(path_data->content_path,
      img_dir, img_name, sizeof(path_data->content_path));

   /* Set core name to "imageviewer" */
   strlcpy(path_data->content_core_name,
         "imageviewer",
         sizeof(path_data->content_core_name));

   /* Set database name (arbitrarily) to "_images_"
    * (required for compatibility with gfx_thumbnail_update_path(),
    * but not actually used...) */
   strlcpy(path_data->content_db_name,
         "_images_", sizeof(path_data->content_db_name));

   /* Redundant error check */
   return *path_data->content_path;
}

/* Sets current thumbnail content to the specified playlist entry.
 * Returns true if content is valid.
 * > Note: It is always best to use playlists when setting
 *   thumbnail content, since there is no guarantee that the
 *   corresponding menu entry label will contain a useful
 *   identifier (it may be 'tainted', e.g. with the current
 *   core name). 'Real' labels should be extracted from source */
bool gfx_thumbnail_set_content_playlist(
      gfx_thumbnail_path_data_t *path_data, playlist_t *playlist, size_t idx)
{
   const char *content_path           = NULL;
   const char *content_label          = NULL;
   const char *core_name              = NULL;
   const char *db_name                = NULL;
   const struct playlist_entry *entry = NULL;

   if (!path_data)
      return false;

   /* When content is updated, must regenerate right/left
    * thumbnail paths */
   path_data->right_path[0]           = '\0';
   path_data->left_path[0]            = '\0';

   /* 'Reset' path_data content strings */
   path_data->content_path[0]         = '\0';
   path_data->content_label_len       = 0;
   path_data->content_label[0]        = '\0';
   path_data->content_core_name[0]    = '\0';
   path_data->content_db_name[0]      = '\0';
   path_data->content_img[0]          = '\0';
   path_data->content_img_full[0]     = '\0';
   path_data->content_img_short[0]    = '\0';

   /* Must also reset playlist thumbnail display modes */
   path_data->playlist_right_mode     = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   path_data->playlist_left_mode      = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   path_data->playlist_index          = 0;

   if (!playlist)
      return false;

   if (idx >= playlist_get_size(playlist))
      return false;

   /* Read playlist values */
   playlist_get_index(playlist, idx, &entry);

   if (!entry)
      return false;

   content_path  = entry->path;
   content_label = entry->label;
   core_name     = entry->core_name;
   db_name       = entry->db_name;

   /* Content without a path is invalid by definition */
   if (!content_path || !*content_path)
      return false;

   /* Cache content path
    * (This is required for imageviewer, history and favourites content) */
   strlcpy(path_data->content_path,
            content_path, sizeof(path_data->content_path));

   /* Cache core name
    * (This is required for imageviewer content) */
   if (core_name && *core_name)
      strlcpy(path_data->content_core_name,
            core_name, sizeof(path_data->content_core_name));

   /* Get content label */
   if (content_label && *content_label)
      path_data->content_label_len = strlcpy(path_data->content_label,
            content_label, sizeof(path_data->content_label));
   else
      path_data->content_label_len = fill_pathname(path_data->content_label,
            path_basename(content_path),
            "", sizeof(path_data->content_label));

   /* Determine content image name */
   {
      char tmp_buf[NAME_MAX_LENGTH];
      fill_pathname(tmp_buf, path_basename(path_data->content_path),
            "", sizeof(tmp_buf));

      gfx_thumbnail_fill_content_img(path_data->content_img_full,
         sizeof(path_data->content_img_full), tmp_buf, false);
      gfx_thumbnail_fill_content_img(path_data->content_img,
         sizeof(path_data->content_img), path_data->content_label, false);

      /* Explicit zero if full name is same as standard name 
         - saves some queries later. */
      if (string_is_equal(path_data->content_img,
          path_data->content_img_full))
         path_data->content_img_full[0] = '\0';

      gfx_thumbnail_fill_content_img(path_data->content_img_short,
         sizeof(path_data->content_img_short), path_data->content_label, true);
   }

   /* Store playlist index */
   path_data->playlist_index = idx;

   /* Redundant error check... */
   if (!*path_data->content_img)
      return false;

   /* Thumbnail image name is done -> now check if
    * per-content database name is defined */
   if (!db_name || !*db_name)
      playlist_get_db_name(playlist, idx, &db_name);
   if (db_name && *db_name)
   {
      /* Hack: There is only one MAME thumbnail repo,
       * so filter any input starting with 'MAME...' */
      if (strncmp(db_name, "MAME", 4) == 0)
      {
         path_data->content_db_name[0] = path_data->content_db_name[2] = 'M';
         path_data->content_db_name[1] = 'A';
         path_data->content_db_name[3] = 'E';
         path_data->content_db_name[4] = '\0';
      }
      else
      {
         char tmp_buf[NAME_MAX_LENGTH];
         const char *pos      = strchr(db_name, '|');
         /* If db_name comes from core info, and there are multiple
          * databases mentioned separated by |, use only first one */
         if (pos && (size_t) (pos - db_name) + 1 < sizeof(tmp_buf))
            strlcpy(tmp_buf, db_name, (size_t)(pos - db_name) + 1);
         else
            strlcpy(tmp_buf, db_name, sizeof(tmp_buf));

         fill_pathname(path_data->content_db_name,
               tmp_buf, "",
               sizeof(path_data->content_db_name));
      }
   }

   /* Playlist entry is valid -> it is now 'safe' to
    * extract any remaining playlist metadata
    * (i.e. thumbnail display modes) */
   path_data->playlist_right_mode =
         playlist_get_thumbnail_mode(playlist, PLAYLIST_THUMBNAIL_RIGHT);
   path_data->playlist_left_mode =
         playlist_get_thumbnail_mode(playlist, PLAYLIST_THUMBNAIL_LEFT);

   return true;
}

/* Updaters */

/* Updates path for specified thumbnail identifier (right, left).
 * Must be called after:
 * - gfx_thumbnail_set_system()
 * - gfx_thumbnail_set_content*()
 * ...and before:
 * - gfx_thumbnail_get_path()
 * Returns true if generated path is valid */
bool gfx_thumbnail_update_path(
      gfx_thumbnail_path_data_t *path_data,
      enum gfx_thumbnail_id thumbnail_id)
{
   char content_dir[DIR_MAX_LENGTH];
   settings_t *settings          = config_get_ptr();
   const char *system_name       = NULL;
   char *thumbnail_path          = NULL;
   const char *dir_thumbnails    = settings->paths.directory_thumbnails;
   bool playlist_allow_non_png   = settings->bools.playlist_allow_non_png;
   unsigned gfx_thumbnails       = settings->uints.gfx_thumbnails;
   unsigned menu_left_thumbnails = settings->uints.menu_left_thumbnails;
   unsigned menu_icon_thumbnails = settings->uints.menu_icon_thumbnails;
   /* Thumbnail extension order. The default (i.e. png) is always the first. */
   #define MAX_SUPPORTED_THUMBNAIL_EXTENSIONS 5
   const char* const SUPPORTED_THUMBNAIL_EXTENSIONS[] = { ".png", ".jpg", ".jpeg", ".bmp", ".tga", 0 };

   if (!path_data)
      return false;

   /* Determine which path we are updating... */
   switch (thumbnail_id)
   {
      case GFX_THUMBNAIL_RIGHT:
         thumbnail_path = path_data->right_path;
         break;
      case GFX_THUMBNAIL_LEFT:
         thumbnail_path = path_data->left_path;
         break;
      case GFX_THUMBNAIL_ICON:
         thumbnail_path = path_data->icon_path;
         break;
      default:
         return false;
   }

   content_dir[0]    = '\0';

   /* Sundry error checking */
   if (!dir_thumbnails || !*dir_thumbnails)
      return false;

   if (!gfx_thumbnail_is_enabled(path_data, thumbnail_id))
      return false;

   /* Generate new path */

   /* > Check path_data for empty strings */
   if (       (!*path_data->content_path)
       ||     (!*path_data->content_img)
       || (   (!*path_data->system)
           && (!*path_data->content_db_name)))
      return false;

   /* > Get current system */
   if (!*path_data->content_db_name)
   {
      /* If this is a content history or favorites playlist
       * then the current 'path_data->system' string is
       * meaningless. In this case, we fall back to the
       * content directory name */
      if (     memcmp(path_data->system, "history", 8) == 0
            || memcmp(path_data->system, "favorites", 10) == 0)
      {
         if (gfx_thumbnail_get_content_dir(
               path_data, content_dir, sizeof(content_dir)) == 0)
            return false;

         system_name = content_dir;
      }
      else
         system_name = path_data->system;
   }
   else
      system_name = path_data->content_db_name;

   /* > Special case: thumbnail for imageviewer content
    *   is the image file itself */
   if (   memcmp(system_name, "images_history", sizeof("images_history")) == 0
       || memcmp(path_data->content_core_name, "imageviewer", sizeof("imageviewer")) == 0)
   {
      /* imageviewer content is identical for left and right thumbnails */
      if (path_is_media_type(path_data->content_path) == RARCH_CONTENT_IMAGE)
         strlcpy(thumbnail_path,
               path_data->content_path, PATH_MAX_LENGTH * sizeof(char));
   }
   else
   {
      int  i;
      char tmp_buf[DIR_MAX_LENGTH];
      const char *type = gfx_thumbnail_get_type(gfx_thumbnails,
            menu_left_thumbnails, menu_icon_thumbnails, path_data, thumbnail_id);
      bool thumbnail_found = false;
      /* > Normal content: assemble path */

      /* >> Base + system name */
      fill_pathname_join_special(thumbnail_path, dir_thumbnails,
            system_name, PATH_MAX_LENGTH * sizeof(char));

      /* >> Add type */
      fill_pathname_join_special(tmp_buf, thumbnail_path, type, sizeof(tmp_buf));

      thumbnail_path[0] = '\0';

      /* >> Add content image - first try with label (database name) */
      if (*path_data->content_img)
      {
         fill_pathname_join_special(thumbnail_path, tmp_buf,
               path_data->content_img, PATH_MAX_LENGTH * sizeof(char));
         thumbnail_found = path_is_valid(thumbnail_path);

         if (playlist_allow_non_png && thumbnail_path && *thumbnail_path)
         {
            for (i = 1; i < MAX_SUPPORTED_THUMBNAIL_EXTENSIONS && !thumbnail_found; i++)
            {
               if (!path_get_extension_mutable(thumbnail_path))
                  continue;

               strlcpy(path_get_extension_mutable(thumbnail_path),
                     SUPPORTED_THUMBNAIL_EXTENSIONS[i], 6);
               thumbnail_found = path_is_valid(thumbnail_path);
            }
         }
      }

      /* >> Add content image - second try with full file name */
      if (   !thumbnail_found 
          && *path_data->content_img_full)
      {
         thumbnail_path[0] = '\0';
         fill_pathname_join_special(thumbnail_path, tmp_buf,
               path_data->content_img_full, PATH_MAX_LENGTH * sizeof(char));
         thumbnail_found = path_is_valid(thumbnail_path);

         if (playlist_allow_non_png && thumbnail_path && *thumbnail_path)
         {
            for (i = 1; i < MAX_SUPPORTED_THUMBNAIL_EXTENSIONS && !thumbnail_found; i++)
            {
               if (!path_get_extension_mutable(thumbnail_path))
                  continue;

               strlcpy(path_get_extension_mutable(thumbnail_path),
                     SUPPORTED_THUMBNAIL_EXTENSIONS[i], 6);
               thumbnail_found = path_is_valid(thumbnail_path);
            }
         }
      }

      /* >> Add content image - third try with shortened name (title only) */
      if (!thumbnail_found && *path_data->content_img_short)
      {
         thumbnail_path[0] = '\0';
         fill_pathname_join_special(thumbnail_path, tmp_buf,
               path_data->content_img_short, PATH_MAX_LENGTH * sizeof(char));
         thumbnail_found = path_is_valid(thumbnail_path);

         if (playlist_allow_non_png && thumbnail_path && *thumbnail_path)
         {
            for (i = 1; i < MAX_SUPPORTED_THUMBNAIL_EXTENSIONS && !thumbnail_found; i++)
            {
               if (!path_get_extension_mutable(thumbnail_path))
                  continue;

               strlcpy(path_get_extension_mutable(thumbnail_path),
                     SUPPORTED_THUMBNAIL_EXTENSIONS[i], 6);
               thumbnail_found = path_is_valid(thumbnail_path);
            }
         }
      }
      /* This logic is valid for locally stored thumbnails. For optional downloads,
       * gfx_thumbnail_get_img_name() is used */
   }

   /* Final error check - is cached path empty? */
   return thumbnail_path && *thumbnail_path;
}

/* Getters */

/* Fetches current content directory.
 * Returns true if content directory is valid. */
size_t gfx_thumbnail_get_content_dir(gfx_thumbnail_path_data_t *path_data,
      char *s, size_t len)
{
   size_t _len;
   char *last_slash;
   char tmp_buf[NAME_MAX_LENGTH];
   if (!path_data || !*path_data->content_path)
      return 0;
   if (!(last_slash = find_last_slash(path_data->content_path)))
      return 0;
   _len = last_slash + 1 - path_data->content_path;
   if (!((_len > 1) && (_len < PATH_MAX_LENGTH)))
      return 0;
   strlcpy(tmp_buf, path_data->content_path, _len * sizeof(char));
   return strlcpy(s, path_basename_nocompression(tmp_buf), len);
}

/* Gets the common savestate thumbnail path. */
void gfx_savestate_thumbnail_get_path(
      char *s, size_t len,
      const char *state_name,
      int state_slot)
{
   size_t _len;

   s[0] = '\0';

   if (!state_name || !*state_name)
      return;

   _len = strlcpy(s, state_name, len);

   if (state_slot > 0)
      _len += snprintf(s + _len, len - _len, "%d", state_slot);
   else if (state_slot < 0)
      _len  = fill_pathname_join_delim(s, state_name, "auto", '.', len);

   strlcpy(s + _len, FILE_PATH_PNG_EXTENSION, len - _len);
}

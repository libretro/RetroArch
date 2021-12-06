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
#include <file/file_path.h>
#include <string/stdstring.h>

#include "gfx_display.h"
#include "gfx_animation.h"

#include "gfx_thumbnail.h"

#include "../tasks/tasks_internal.h"

#define DEFAULT_GFX_THUMBNAIL_STREAM_DELAY  83.333333f
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

   if (!thumbnail)
      return;

   thumbnail->fade_active = false;
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
   if ((thumbnail->status == GFX_THUMBNAIL_STATUS_AVAILABLE) ||
       (p_gfx_thumb->fade_missing &&
            (thumbnail->status == GFX_THUMBNAIL_STATUS_MISSING)))
   {
      if (p_gfx_thumb->fade_duration > 0.0f)
      {
         gfx_animation_ctx_entry_t animation_entry;

         thumbnail->alpha                 = 0.0f;
         thumbnail->fade_active           = true;

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
      gfx_thumbnail_path_data_t *path_data, enum gfx_thumbnail_id thumbnail_id,
      playlist_t *playlist, size_t idx, gfx_thumbnail_t *thumbnail,
      unsigned gfx_thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails
      )
{
   const char *thumbnail_path         = NULL;
   bool has_thumbnail                 = false;
   gfx_thumbnail_state_t *p_gfx_thumb = NULL;
   p_gfx_thumb                        = NULL;
   
   if (!path_data || !thumbnail)
      return;

   p_gfx_thumb                        = &gfx_thumb_st;

   /* Reset thumbnail, then set 'missing' status by default
    * (saves a number of checks later) */
   gfx_thumbnail_reset(thumbnail);
   thumbnail->status = GFX_THUMBNAIL_STATUS_MISSING;

   /* Update/extract thumbnail path */
   if (gfx_thumbnail_is_enabled(path_data, thumbnail_id))
      if (gfx_thumbnail_update_path(path_data, thumbnail_id))
         has_thumbnail = gfx_thumbnail_get_path(path_data, thumbnail_id, &thumbnail_path);

   /* Load thumbnail, if required */
   if (has_thumbnail)
   {
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
         const char *system                         = NULL;
         const char *img_name                       = NULL;
         static char last_img_name[PATH_MAX_LENGTH] = {0};

         if (!playlist)
            goto end;

         /* Get current image name */
         if (!gfx_thumbnail_get_img_name(path_data, &img_name))
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
         if (string_is_equal(img_name, last_img_name))
            goto end;

         strlcpy(last_img_name, img_name, sizeof(last_img_name));

         /* Get system name */
         if (!gfx_thumbnail_get_system(path_data, &system))
            goto end;

         /* Trigger thumbnail download */
         task_push_pl_entry_thumbnail_download(
               system, playlist, (unsigned)idx,
               false, true);
      }
#endif
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
      unsigned gfx_thumbnail_upscale_threshold
      )
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
   if (string_is_empty(file_path))
      return;

   if (!path_is_valid(file_path))
      return;

   /* Load thumbnail */
   thumbnail_tag = (gfx_thumbnail_tag_t*)malloc(sizeof(gfx_thumbnail_tag_t));

   if (!thumbnail_tag)
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
   if (thumbnail->fade_active)
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
   thumbnail->fade_active = false;
}

/* Stream processing */

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
      playlist_t *playlist,
      size_t idx,
      gfx_thumbnail_t *thumbnail,
      bool on_screen,
      unsigned gfx_thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails
      )
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
	 gfx_thumbnail_state_t *p_gfx_thumb  = &gfx_thumb_st;

         /* Check if stream delay timer has elapsed */
         thumbnail->delay_timer             += p_anim->delta_time;

         if (thumbnail->delay_timer > p_gfx_thumb->stream_delay)
         {
            /* Update thumbnail content */
            if (!path_data ||
                !playlist ||
                !gfx_thumbnail_set_content_playlist(path_data, playlist, idx))
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
                  network_on_demand_thumbnails
                  );
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
      bool network_on_demand_thumbnails
      )
{
   if (!right_thumbnail || !left_thumbnail)
      return;

   if (on_screen)
   {
      /* Entry is on-screen
       * > Only process if current status is
       *   GFX_THUMBNAIL_STATUS_UNKNOWN */
      bool process_right = (right_thumbnail->status == GFX_THUMBNAIL_STATUS_UNKNOWN);
      bool process_left  = (left_thumbnail->status  == GFX_THUMBNAIL_STATUS_UNKNOWN);

      if (process_right || process_left)
      {
         /* Check if stream delay timer has elapsed */
	 gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;
         float delta_time                   = p_anim->delta_time;
         bool request_right                 = false;
         bool request_left                  = false;

         if (process_right)
         {
            right_thumbnail->delay_timer += delta_time;
            request_right                 =
                  (right_thumbnail->delay_timer > p_gfx_thumb->stream_delay);
         }

         if (process_left)
         {
            left_thumbnail->delay_timer  += delta_time;
            request_left                  =
                  (left_thumbnail->delay_timer > p_gfx_thumb->stream_delay);
         }

         /* Check if one or more thumbnails should be requested */
         if (request_right || request_left)
         {
            /* Update thumbnail content */
            if (!path_data ||
                !playlist ||
                !gfx_thumbnail_set_content_playlist(path_data, playlist, idx))
            {
               /* Content is invalid
                * > Reset thumbnail and set missing status */
               if (request_right)
               {
                  gfx_thumbnail_reset(right_thumbnail);
                  right_thumbnail->status = GFX_THUMBNAIL_STATUS_MISSING;
                  right_thumbnail->alpha  = 1.0f;
               }

               if (request_left)
               {
                  gfx_thumbnail_reset(left_thumbnail);
                  left_thumbnail->status  = GFX_THUMBNAIL_STATUS_MISSING;
                  left_thumbnail->alpha   = 1.0f;
               }

               return;
            }

            /* Request image load */
            if (request_right)
               gfx_thumbnail_request(
                     path_data, GFX_THUMBNAIL_RIGHT, playlist, idx, right_thumbnail,
                     gfx_thumbnail_upscale_threshold,
                     network_on_demand_thumbnails);

            if (request_left)
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
   float display_aspect;
   float thumbnail_aspect;

   /* Sanity check */
   if (!thumbnail || (width < 1) || (height < 1))
      goto error;

   if ((thumbnail->width < 1) || (thumbnail->height < 1))
      goto error;

   /* Account for display/thumbnail aspect ratio
    * differences */
   display_aspect   = (float)width            / (float)height;
   thumbnail_aspect = (float)thumbnail->width / (float)thumbnail->height;

   if (thumbnail_aspect > display_aspect)
   {
      *draw_width  = (float)width;
      *draw_height = (float)thumbnail->height * (*draw_width / (float)thumbnail->width);
   }
   else
   {
      *draw_height = (float)height;
      *draw_width  = (float)thumbnail->width * (*draw_height / (float)thumbnail->height);
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
   return;

error:
   *draw_width  = 0.0f;
   *draw_height = 0.0f;
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
   if (!thumbnail ||
       (width < 1) || (height < 1) || (alpha <= 0.0f) || (scale_factor <= 0.0f))
      return;
   if (!dispctx)
      return;

   /* Only draw thumbnail if it is available... */
   if (thumbnail->status == GFX_THUMBNAIL_STATUS_AVAILABLE)
   {
      gfx_display_ctx_rotate_draw_t rotate_draw;
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

      /* Perform 'rotation' step
       * > Note that rotation does not actually work...
       * > It rotates the image all right, but distorts it
       *   to fit the aspect of the bounding box while clipping
       *   off any 'corners' that extend beyond the bounding box
       * > Since the result is visual garbage, we disable
       *   rotation entirely
       * > But we still have to call gfx_display_rotate_z(),
       *   or nothing will be drawn...
       * Note that we also disable scaling here (scale_enable),
       * since we handle scaling internally... */
      rotate_draw.matrix       = &mymat;
      rotate_draw.rotation     = 0.0f;
      rotate_draw.scale_x      = 1.0f;
      rotate_draw.scale_y      = 1.0f;
      rotate_draw.scale_z      = 1.0f;
      rotate_draw.scale_enable = false;

      gfx_display_rotate_z(p_disp, &rotate_draw, userdata);

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
         if ((shadow->type != GFX_THUMBNAIL_SHADOW_NONE) &&
               (shadow->alpha > 0.0f))
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

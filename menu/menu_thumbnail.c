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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <features/features_cpu.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#include "menu_animation.h"
#include "menu_driver.h"

#include "menu_thumbnail.h"

#include "../retroarch.h"
#include "../tasks/tasks_internal.h"

/* When streaming thumbnails, to minimise the processing
 * of unnecessary images (i.e. when scrolling rapidly through
 * playlists), we delay loading until an entry has been on screen
 * for at least menu_thumbnail_delay ms */
#define DEFAULT_MENU_THUMBNAIL_STREAM_DELAY 83.333333f
static float menu_thumbnail_stream_delay = DEFAULT_MENU_THUMBNAIL_STREAM_DELAY;

/* Duration in ms of the thumbnail 'fade in' animation */
#define DEFAULT_MENU_THUMBNAIL_FADE_DURATION 166.66667f
static float menu_thumbnail_fade_duration = DEFAULT_MENU_THUMBNAIL_FADE_DURATION;

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
static uint64_t menu_thumbnail_list_id = 0;

/* Utility structure, sent as userdata when pushing
 * an image load */
typedef struct
{
   menu_thumbnail_t *thumbnail;
   retro_time_t list_id;
} menu_thumbnail_tag_t;

/* Setters */

/* When streaming thumbnails, sets time in ms that an
 * entry must be on screen before an image load is
 * requested */
void menu_thumbnail_set_stream_delay(float delay)
{
   menu_thumbnail_stream_delay = (delay >= 0.0f) ?
         delay : DEFAULT_MENU_THUMBNAIL_STREAM_DELAY;
}

/* Sets duration in ms of the thumbnail 'fade in'
 * animation */
void menu_thumbnail_set_fade_duration(float duration)
{
   menu_thumbnail_fade_duration = (duration >= 0.0f) ?
         duration : DEFAULT_MENU_THUMBNAIL_FADE_DURATION;
}

/* Getters */

/* Fetches current streaming thumbnails request delay */
float menu_thumbnail_get_stream_delay(void)
{
   return menu_thumbnail_stream_delay;
}

/* Fetches current 'fade in' animation duration */
float menu_thumbnail_get_fade_duration(void)
{
   return menu_thumbnail_fade_duration;
}

/* Callbacks */

/* Used to process thumbnail data following completion
 * of image load task */
static void menu_thumbnail_handle_upload(
      retro_task_t *task, void *task_data, void *user_data, const char *err)
{
   struct texture_image *img           = (struct texture_image*)task_data;
   menu_thumbnail_tag_t *thumbnail_tag = (menu_thumbnail_tag_t*)user_data;
   menu_animation_ctx_entry_t animation_entry;

   /* Sanity check */
   if (!thumbnail_tag)
      goto end;

   /* Ensure that we are operating on the correct
    * thumbnail... */
   if (thumbnail_tag->list_id != menu_thumbnail_list_id)
      goto end;

   /* Only process image if we are waiting for it */
   if (thumbnail_tag->thumbnail->status != MENU_THUMBNAIL_STATUS_PENDING)
      goto end;

   /* Sanity check: if thumbnail already has a texture,
    * we're in some kind of weird error state - in this
    * case, the best course of action is to just reset
    * the thumbnail... */
   if (thumbnail_tag->thumbnail->texture)
      menu_thumbnail_reset(thumbnail_tag->thumbnail);

   /* Set thumbnail 'missing' status by default
    * (saves a number of checks later) */
   thumbnail_tag->thumbnail->status = MENU_THUMBNAIL_STATUS_MISSING;

   /* Check we have a valid image */
   if (!img)
      goto end;

   if ((img->width < 1) || (img->height < 1))
      goto end;

   /* Upload texture to GPU */
   if (!video_driver_texture_load(
            img, TEXTURE_FILTER_MIPMAP_LINEAR, &thumbnail_tag->thumbnail->texture))
      goto end;

   /* Cache dimensions */
   thumbnail_tag->thumbnail->width  = img->width;
   thumbnail_tag->thumbnail->height = img->height;

   /* Update thumbnail status */
   thumbnail_tag->thumbnail->status = MENU_THUMBNAIL_STATUS_AVAILABLE;

   /* Trigger 'fade in' animation, if required */
   if (menu_thumbnail_fade_duration > 0.0f)
   {
      thumbnail_tag->thumbnail->alpha  = 0.0f;

      animation_entry.easing_enum      = EASING_OUT_QUAD;
      animation_entry.tag              = (uintptr_t)&thumbnail_tag->thumbnail->alpha;
      animation_entry.duration         = menu_thumbnail_fade_duration;
      animation_entry.target_value     = 1.0f;
      animation_entry.subject          = &thumbnail_tag->thumbnail->alpha;
      animation_entry.cb               = NULL;
      animation_entry.userdata         = NULL;

      menu_animation_push(&animation_entry);
   }
   else
      thumbnail_tag->thumbnail->alpha  = 1.0f;

end:
   /* Clean up */
   if (img)
   {
      image_texture_free(img);
      free(img);
   }

   if (thumbnail_tag)
      free(thumbnail_tag);
}

/* Core interface */

/* When called, prevents the handling of any pending
 * thumbnail load requests
 * >> **MUST** be called before deleting any menu_thumbnail_t
 *    objects passed to menu_thumbnail_request() or
 *    menu_thumbnail_process_stream(), otherwise
 *    heap-use-after-free errors *will* occur */
void menu_thumbnail_cancel_pending_requests(void)
{
   menu_thumbnail_list_id++;
}

/* Requests loading of the specified thumbnail
 * - If operation fails, 'thumbnail->status' will be set to
 *   MENU_THUMBNAIL_STATUS_MISSING
 * - If operation is successful, 'thumbnail->status' will be
 *   set to MENU_THUMBNAIL_STATUS_PENDING
 * 'thumbnail' will be populated with texture info/metadata
 * once the image load is complete
 * NOTE 1: Must be called *after* menu_thumbnail_set_system()
 *         and menu_thumbnail_set_content*()
 * NOTE 2: 'playlist' and 'idx' are only required here for
 *         on-demand thumbnail download support
 *         (an annoyance...) */ 
void menu_thumbnail_request(
      menu_thumbnail_path_data_t *path_data, enum menu_thumbnail_id thumbnail_id,
      playlist_t *playlist, size_t idx, menu_thumbnail_t *thumbnail,
      unsigned menu_thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails
      )
{
   const char *thumbnail_path          = NULL;
   bool has_thumbnail                  = false;

   if (!path_data || !thumbnail)
      return;

   /* Reset thumbnail, then set 'missing' status by default
    * (saves a number of checks later) */
   menu_thumbnail_reset(thumbnail);
   thumbnail->status = MENU_THUMBNAIL_STATUS_MISSING;

   /* Update/extract thumbnail path */
   if (menu_thumbnail_is_enabled(path_data, thumbnail_id))
      if (menu_thumbnail_update_path(path_data, thumbnail_id))
         has_thumbnail = menu_thumbnail_get_path(path_data, thumbnail_id, &thumbnail_path);

   /* Load thumbnail, if required */
   if (has_thumbnail)
   {
      if (path_is_valid(thumbnail_path))
      {
         menu_thumbnail_tag_t *thumbnail_tag =
               (menu_thumbnail_tag_t*)calloc(1, sizeof(menu_thumbnail_tag_t));

         if (!thumbnail_tag)
            return;

         /* Configure user data */
         thumbnail_tag->thumbnail = thumbnail;
         thumbnail_tag->list_id   = menu_thumbnail_list_id;

         /* Would like to cancel any existing image load tasks
          * here, but can't see how to do it... */
         if(task_push_image_load(
               thumbnail_path, video_driver_supports_rgba(),
               menu_thumbnail_upscale_threshold,
               menu_thumbnail_handle_upload, thumbnail_tag))
            thumbnail->status = MENU_THUMBNAIL_STATUS_PENDING;
      }
#ifdef HAVE_NETWORKING
      /* Handle on demand thumbnail downloads */
      else if (network_on_demand_thumbnails)
      {
         const char *system                         = NULL;
         const char *img_name                       = NULL;
         static char last_img_name[PATH_MAX_LENGTH] = {0};

         if (!playlist)
            return;

         /* Get current image name */
         if (!menu_thumbnail_get_img_name(path_data, &img_name))
            return;

         /* Only trigger a thumbnail download if image
          * name has changed since the last call of
          * menu_thumbnail_request()
          * > Allows menu_thumbnail_request() to be used
          *   for successive right/left thumbnail requests
          *   with minimal duplication of effort
          *   (i.e. task_push_pl_entry_thumbnail_download()
          *   will automatically cancel if a download for the
          *   existing playlist entry is pending, but the
          *   checks required for this involve significant
          *   overheads. We can avoid this entirely with
          *   a simple string comparison) */
         if (string_is_equal(img_name, last_img_name))
            return;

         strlcpy(last_img_name, img_name, sizeof(last_img_name));

         /* Get system name */
         if (!menu_thumbnail_get_system(path_data, &system))
            return;

         /* Trigger thumbnail download */
         task_push_pl_entry_thumbnail_download(
               system, playlist, (unsigned)idx,
               false, true);
      }
#endif
   }
}

/* Requests loading of a specific thumbnail image file
 * (may be used, for example, to load savestate images)
 * - If operation fails, 'thumbnail->status' will be set to
 *   MUI_THUMBNAIL_STATUS_MISSING
 * - If operation is successful, 'thumbnail->status' will be
 *   set to MUI_THUMBNAIL_STATUS_PENDING
 * 'thumbnail' will be populated with texture info/metadata
 * once the image load is complete */
void menu_thumbnail_request_file(
      const char *file_path, menu_thumbnail_t *thumbnail,
      unsigned menu_thumbnail_upscale_threshold
      )
{
   menu_thumbnail_tag_t *thumbnail_tag = NULL;

   if (!thumbnail)
      return;

   /* Reset thumbnail, then set 'missing' status by default
    * (saves a number of checks later) */
   menu_thumbnail_reset(thumbnail);
   thumbnail->status = MENU_THUMBNAIL_STATUS_MISSING;

   /* Check if file path is valid */
   if (string_is_empty(file_path))
      return;

   if (!path_is_valid(file_path))
      return;

   /* Load thumbnail */
   thumbnail_tag = (menu_thumbnail_tag_t*)calloc(1, sizeof(menu_thumbnail_tag_t));

   if (!thumbnail_tag)
      return;

   /* Configure user data */
   thumbnail_tag->thumbnail = thumbnail;
   thumbnail_tag->list_id   = menu_thumbnail_list_id;

   /* Would like to cancel any existing image load tasks
    * here, but can't see how to do it... */
   if(task_push_image_load(
         file_path, video_driver_supports_rgba(),
         menu_thumbnail_upscale_threshold,
         menu_thumbnail_handle_upload, thumbnail_tag))
      thumbnail->status = MENU_THUMBNAIL_STATUS_PENDING;
}

/* Resets (and free()s the current texture of) the
 * specified thumbnail */
void menu_thumbnail_reset(menu_thumbnail_t *thumbnail)
{
   if (!thumbnail)
      return;

   if (thumbnail->texture)
   {
      menu_animation_ctx_tag tag = (uintptr_t)&thumbnail->alpha;

      /* Unload texture */
      video_driver_texture_unload(&thumbnail->texture);

      /* Ensure any 'fade in' animation is killed */
      menu_animation_kill_by_tag(&tag);
   }

   /* Reset all parameters */
   thumbnail->status      = MENU_THUMBNAIL_STATUS_UNKNOWN;
   thumbnail->texture     = 0;
   thumbnail->width       = 0;
   thumbnail->height      = 0;
   thumbnail->alpha       = 0.0f;
   thumbnail->delay_timer = 0.0f;
}

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
      playlist_t *playlist, size_t idx, menu_thumbnail_t *thumbnail, bool on_screen,
      unsigned menu_thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails
      )
{
   if (!thumbnail)
      return;

   if (on_screen)
   {
      /* Entry is on-screen
       * > Only process if current status is
       *   MENU_THUMBNAIL_STATUS_UNKNOWN */
      if (thumbnail->status == MENU_THUMBNAIL_STATUS_UNKNOWN)
      {
         /* Check if stream delay timer has elapsed */
         thumbnail->delay_timer += menu_animation_get_delta_time();

         if (thumbnail->delay_timer > menu_thumbnail_stream_delay)
         {
            /* Sanity check */
            if (!path_data || !playlist)
               return;

            /* Update thumbnail content */
            if (!menu_thumbnail_set_content_playlist(path_data, playlist, idx))
            {
               /* Content is invalid
                * > Reset thumbnail and set missing status */
               menu_thumbnail_reset(thumbnail);
               thumbnail->status = MENU_THUMBNAIL_STATUS_MISSING;
               return;
            }

            /* Request image load */
            menu_thumbnail_request(
                  path_data, thumbnail_id, playlist, idx, thumbnail,
                  menu_thumbnail_upscale_threshold,
                  network_on_demand_thumbnails
                  );
         }
      }
   }
   else
   {
      /* Entry is off-screen
       * > If status is MENU_THUMBNAIL_STATUS_UNKNOWN,
       *   thumbnail is already in a blank state - but we
       *   must ensure that delay timer is set to zero */
      if (thumbnail->status == MENU_THUMBNAIL_STATUS_UNKNOWN)
         thumbnail->delay_timer = 0.0f;
      /* In all other cases, reset thumbnail */
      else
         menu_thumbnail_reset(thumbnail);
   }
}

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
      bool on_screen,
      unsigned menu_thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails
      )
{
   if (!right_thumbnail || !left_thumbnail)
      return;

   if (on_screen)
   {
      /* Entry is on-screen
       * > Only process if current status is
       *   MENU_THUMBNAIL_STATUS_UNKNOWN */
      bool process_right = (right_thumbnail->status == MENU_THUMBNAIL_STATUS_UNKNOWN);
      bool process_left  = (left_thumbnail->status  == MENU_THUMBNAIL_STATUS_UNKNOWN);

      if (process_right || process_left)
      {
         /* Check if stream delay timer has elapsed */
         float delta_time   = menu_animation_get_delta_time();
         bool request_right = false;
         bool request_left  = false;

         if (process_right)
         {
            right_thumbnail->delay_timer += delta_time;
            request_right                 =
                  (right_thumbnail->delay_timer > menu_thumbnail_stream_delay);
         }

         if (process_left)
         {
            left_thumbnail->delay_timer  += delta_time;
            request_left                  =
                  (left_thumbnail->delay_timer > menu_thumbnail_stream_delay);
         }

         /* Check if one or more thumbnails should be requested */
         if (request_right || request_left)
         {
            /* Sanity check */
            if (!path_data || !playlist)
               return;

            /* Update thumbnail content */
            if (!menu_thumbnail_set_content_playlist(path_data, playlist, idx))
            {
               /* Content is invalid
                * > Reset thumbnail and set missing status */
               if (request_right)
               {
                  menu_thumbnail_reset(right_thumbnail);
                  right_thumbnail->status = MENU_THUMBNAIL_STATUS_MISSING;
               }

               if (request_left)
               {
                  menu_thumbnail_reset(left_thumbnail);
                  left_thumbnail->status  = MENU_THUMBNAIL_STATUS_MISSING;
               }

               return;
            }

            /* Request image load */
            if (request_right)
               menu_thumbnail_request(
                     path_data, MENU_THUMBNAIL_RIGHT, playlist, idx, right_thumbnail,
                     menu_thumbnail_upscale_threshold,
                     network_on_demand_thumbnails);

            if (request_left)
               menu_thumbnail_request(
                     path_data, MENU_THUMBNAIL_LEFT, playlist, idx, left_thumbnail,
                     menu_thumbnail_upscale_threshold,
                     network_on_demand_thumbnails);
         }
      }
   }
   else
   {
      /* Entry is off-screen
       * > If status is MENU_THUMBNAIL_STATUS_UNKNOWN,
       *   thumbnail is already in a blank state - but we
       *   must ensure that delay timer is set to zero
       * > In all other cases, reset thumbnail */
      if (right_thumbnail->status == MENU_THUMBNAIL_STATUS_UNKNOWN)
         right_thumbnail->delay_timer = 0.0f;
      else
         menu_thumbnail_reset(right_thumbnail);

      if (left_thumbnail->status == MENU_THUMBNAIL_STATUS_UNKNOWN)
         left_thumbnail->delay_timer = 0.0f;
      else
         menu_thumbnail_reset(left_thumbnail);
   }
}

/* Thumbnail rendering */

/* Determines the actual screen dimensions of a
 * thumbnail when centred with aspect correct
 * scaling within a rectangle of (width x height) */
void menu_thumbnail_get_draw_dimensions(
      menu_thumbnail_t *thumbnail,
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
    * > Side note: We cannot use the menu_display_ctx_draw_t
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
   return;
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
void menu_thumbnail_draw(
      video_frame_info_t *video_info, menu_thumbnail_t *thumbnail,
      float x, float y, unsigned width, unsigned height,
      enum menu_thumbnail_alignment alignment,
      float alpha, float scale_factor,
      menu_thumbnail_shadow_t *shadow)
{
   /* Sanity check */
   if (!video_info || !thumbnail ||
       (width < 1) || (height < 1) || (alpha <= 0.0f) || (scale_factor <= 0.0f))
      return;

   /* Only draw thumbnail if it is available... */
   if (thumbnail->status == MENU_THUMBNAIL_STATUS_AVAILABLE)
   {
      menu_display_ctx_rotate_draw_t rotate_draw;
      menu_display_ctx_draw_t draw;
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
      else if (thumbnail_alpha < 1.0f)
         menu_display_set_alpha(thumbnail_color, thumbnail_alpha);

      /* Get thumbnail dimensions */
      menu_thumbnail_get_draw_dimensions(
            thumbnail, width, height, scale_factor,
            &draw_width, &draw_height);

      menu_display_blend_begin(video_info);

      /* Perform 'rotation' step
       * > Note that rotation does not actually work...
       * > It rotates the image all right, but distorts it
       *   to fit the aspect of the bounding box while clipping
       *   off any 'corners' that extend beyond the bounding box
       * > Since the result is visual garbage, we disable
       *   rotation entirely
       * > But we still have to call menu_display_rotate_z(),
       *   or nothing will be drawn...
       * Note that we also disable scaling here (scale_enable),
       * since we handle scaling internally... */
      rotate_draw.matrix       = &mymat;
      rotate_draw.rotation     = 0.0f;
      rotate_draw.scale_x      = 1.0f;
      rotate_draw.scale_y      = 1.0f;
      rotate_draw.scale_z      = 1.0f;
      rotate_draw.scale_enable = false;

      menu_display_rotate_z(&rotate_draw, video_info);

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
      draw.prim_type       = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
      draw.pipeline.id     = 0;

      /* Set thumbnail alignment within bounding box */
      switch (alignment)
      {
         case MENU_THUMBNAIL_ALIGN_TOP:
            /* Centred horizontally */
            draw_x = x + ((float)width - draw_width) / 2.0f;
            /* Drawn at top of bounding box */
            draw_y = (float)video_info->height - y - draw_height;
            break;
         case MENU_THUMBNAIL_ALIGN_BOTTOM:
            /* Centred horizontally */
            draw_x = x + ((float)width - draw_width) / 2.0f;
            /* Drawn at bottom of bounding box */
            draw_y = (float)video_info->height - y - (float)height;
            break;
         case MENU_THUMBNAIL_ALIGN_LEFT:
            /* Drawn at left side of bounding box */
            draw_x = x;
            /* Centred vertically */
            draw_y = (float)video_info->height - y - draw_height - ((float)height - draw_height) / 2.0f;
            break;
         case MENU_THUMBNAIL_ALIGN_RIGHT:
            /* Drawn at right side of bounding box */
            draw_x = x + (float)width - draw_width;
            /* Centred vertically */
            draw_y = (float)video_info->height - y - draw_height - ((float)height - draw_height) / 2.0f;
            break;
         case MENU_THUMBNAIL_ALIGN_CENTRE:
         default:
            /* Centred both horizontally and vertically */
            draw_x = x + ((float)width - draw_width) / 2.0f;
            draw_y = (float)video_info->height - y - draw_height - ((float)height - draw_height) / 2.0f;
            break;
      }

      /* Draw shadow effect, if required */
      if (shadow)
      {
         /* Sanity check */
         if ((shadow->type != MENU_THUMBNAIL_SHADOW_NONE) &&
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

            menu_display_set_alpha(shadow_color, shadow_alpha);

            /* Configure shadow based on effect type
             * > Not using a switch() here, since we've
             *   already eliminated MENU_THUMBNAIL_SHADOW_NONE */
            if (shadow->type == MENU_THUMBNAIL_SHADOW_OUTLINE)
            {
               shadow_width  = draw_width  + (float)(shadow->outline.width * 2);
               shadow_height = draw_height + (float)(shadow->outline.width * 2);
               shadow_x      = draw_x - (float)shadow->outline.width;
               shadow_y      = draw_y - (float)shadow->outline.width;
            }
            /* Default: MENU_THUMBNAIL_SHADOW_DROP */
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
            menu_display_draw(&draw, video_info);
         }
      }

      /* Final thumbnail draw object configuration */
      coords.color = (const float*)thumbnail_color;
      draw.width   = (unsigned)draw_width;
      draw.height  = (unsigned)draw_height;
      draw.x       = draw_x;
      draw.y       = draw_y;

      /* Draw thumbnail */
      menu_display_draw(&draw, video_info);
      menu_display_blend_end(video_info);
   }
}

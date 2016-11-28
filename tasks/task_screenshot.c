/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2016 - Brad Parker
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef _XBOX1
#include <xtl.h>
#include <xgraphics.h>
#endif

#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include <boolean.h>
#include <stdint.h>
#include <string.h>

#include <file/file_path.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>

#ifdef HAVE_RBMP
#include <formats/rbmp.h>
#endif

#ifdef HAVE_RPNG
#include <formats/rpng.h>
#define IMG_EXT "png"
#else
#define IMG_EXT "bmp"
#endif

#include "../defaults.h"
#include "../configuration.h"
#include "../runloop.h"
#include "../paths.h"
#include "../msg_hash.h"

#include "../gfx/video_driver.h"

#include "tasks_internal.h"

typedef struct
{
#ifdef _XBOX1
   D3DSurface *surf;
#endif
   char filename[PATH_MAX_LENGTH];
   char shotname[256];
#ifdef HAVE_RPNG
   uint8_t *out_buffer;
   struct scaler_ctx scaler;
#endif
   const void *frame;
   unsigned width;
   unsigned height;
   int pitch;
   bool bgr24;
   void *userbuf;
} screenshot_task_state_t;

/**
 * task_screenshot_handler:
 * @task : the task being worked on
 *
 * Saves a screenshot to disk.
 **/
static void task_screenshot_handler(retro_task_t *task)
{
   screenshot_task_state_t *state = (screenshot_task_state_t*)task->state;
   char *msg                      = NULL;
   bool is_paused                 = runloop_ctl(RUNLOOP_CTL_IS_PAUSED, NULL);
   bool ret                       = false;

   if (task->progress == 100)
   {
      task->finished = true;

      if (state->userbuf)
         free(state->userbuf);

      free(state);
      return;
   }

#if defined(_XBOX1)
   if (XGWriteSurfaceToFile(state->surf, state->filename) == S_OK)
      ret = true;
   state->surf->Release();
#elif defined(HAVE_RPNG)
   {
      struct scaler_ctx *scaler = (struct scaler_ctx*)&state->scaler;

      if (state->bgr24)
         scaler->in_fmt   = SCALER_FMT_BGR24;
      else if (video_driver_get_pixel_format() == RETRO_PIXEL_FORMAT_XRGB8888)
         scaler->in_fmt   = SCALER_FMT_ARGB8888;
      else
         scaler->in_fmt   = SCALER_FMT_RGB565;

      video_frame_convert_to_bgr24(
            scaler,
            state->out_buffer,
            (const uint8_t*)state->frame + ((int)state->height - 1) * state->pitch,
            state->width, state->height,
            -state->pitch);
   }

   scaler_ctx_gen_reset(&state->scaler);

   ret = rpng_save_image_bgr24(
         state->filename,
         state->out_buffer,
         state->width,
         state->height,
         state->width * 3
         );

   free(state->out_buffer);
#elif defined(HAVE_RBMP)
   enum rbmp_source_type bmp_type = RBMP_SOURCE_TYPE_DONT_CARE;

   if (state->bgr24)
      bmp_type = RBMP_SOURCE_TYPE_BGR24;
   else if (video_driver_get_pixel_format() == RETRO_PIXEL_FORMAT_XRGB8888)
      bmp_type = RBMP_SOURCE_TYPE_XRGB888;

   ret = rbmp_save_image(state->filename,
         state->frame,
         state->width,
         state->height,
         state->pitch,
         bmp_type);
#endif

#ifdef HAVE_IMAGEVIEWER
   if (ret)
      if (content_push_to_history_playlist(g_defaults.image_history, state->filename,
               "imageviewer", "builtin"))
         playlist_write_file(g_defaults.image_history);
#endif

   task->progress = 100;

   if (!ret)
   {
      msg = strdup(msg_hash_to_str(MSG_FAILED_TO_TAKE_SCREENSHOT));
      runloop_msg_queue_push(msg, 1, is_paused ? 1 : 180, true);
   }
}

/* Take frame bottom-up. */
static bool screenshot_dump(
      const char *name_base,
      const char *folder,
      const void *frame,
      unsigned width,
      unsigned height,
      int pitch, bool bgr24, void *userbuf, bool savestate)
{
#ifdef _XBOX1
   d3d_video_t *d3d               = (d3d_video_t*)video_driver_get_ptr(true);
#endif
   settings_t *settings           = config_get_ptr();
   retro_task_t *task             = (retro_task_t*)calloc(1, sizeof(*task));
   screenshot_task_state_t *state = (screenshot_task_state_t*)
         calloc(1, sizeof(*state));

   state->bgr24   = bgr24;
   state->height  = height;
   state->width   = width;
   state->pitch   = pitch;
   state->frame   = frame;
   state->userbuf = userbuf;

   if (settings->auto_screenshot_filename && !savestate)
      fill_str_dated_filename(state->shotname, path_basename(name_base),
            IMG_EXT, sizeof(state->shotname));
   else
      snprintf(state->shotname, sizeof(state->shotname), "%s.png", path_basename(name_base));

   fill_pathname_join(state->filename, folder, state->shotname, sizeof(state->filename));

#ifdef _XBOX1
   d3d->dev->GetBackBuffer(-1, D3DBACKBUFFER_TYPE_MONO, &state->surf);
#elif defined(HAVE_RPNG)
   state->out_buffer = (uint8_t*)malloc(width * height * 3);
   if (!state->out_buffer)
   {
      if (task)
         free(task);
      free(state);
      return false;
   }
#endif

   task->type    = TASK_TYPE_BLOCKING;
   task->state   = state;
   task->handler = task_screenshot_handler;
   task->title   = strdup(msg_hash_to_str(MSG_TAKING_SCREENSHOT));
   task_queue_ctl(TASK_QUEUE_CTL_PUSH, task);

   return true;
}

#if !defined(VITA)
static bool take_screenshot_viewport(const char *name_base, bool savestate)
{
   char screenshot_path[PATH_MAX_LENGTH];
   const char *screenshot_dir            = NULL;
   uint8_t *buffer                       = NULL;
   bool retval                           = false;
   struct video_viewport vp              = {0};
   settings_t *settings                  = config_get_ptr();

   screenshot_path[0]                    = '\0';

   video_driver_get_viewport_info(&vp);

   if (!vp.width || !vp.height)
      return false;

   buffer = (uint8_t*)malloc(vp.width * vp.height * 3);

   if (!buffer)
      return false;

   if (!video_driver_read_viewport(buffer))
      goto error;

   screenshot_dir = settings->directory.screenshot;

   if (string_is_empty(screenshot_dir))
   {
      fill_pathname_basedir(screenshot_path, name_base,
            sizeof(screenshot_path));
      screenshot_dir = screenshot_path;
   }

   /* Data read from viewport is in bottom-up order, suitable for BMP. */
   if (!screenshot_dump(name_base, screenshot_dir, buffer, vp.width, vp.height,
            vp.width * 3, true, buffer, savestate))
      goto error;

   return true;

error:
   if (buffer)
      free(buffer);
   return retval;
}
#endif

static bool take_screenshot_raw(const char *name_base, void *userbuf,
      bool savestate)
{
   size_t pitch;
   unsigned width, height;
   char screenshot_path[PATH_MAX_LENGTH];
   const void *data                      = NULL;
   settings_t *settings                  = config_get_ptr();
   const char *screenshot_dir            = settings->directory.screenshot;

   screenshot_path[0]                    = '\0';

   video_driver_cached_frame_get(&data, &width, &height, &pitch);

   if (string_is_empty(settings->directory.screenshot))
   {
      fill_pathname_basedir(screenshot_path, name_base,
            sizeof(screenshot_path));
      screenshot_dir = screenshot_path;
   }

   /* Negative pitch is needed as screenshot takes bottom-up,
    * but we use top-down.
    */
   if (!screenshot_dump(name_base, screenshot_dir,
         (const uint8_t*)data + (height - 1) * pitch,
         width, height, -pitch, false, userbuf, savestate))
      return false;

   return true;
}

static bool take_screenshot_choice(const char *name_base, bool savestate)
{
   settings_t *settings = config_get_ptr();

   /* No way to infer screenshot directory. */
   if (     string_is_empty(settings->directory.screenshot)
         && string_is_empty(name_base))
      return false;

   if (video_driver_supports_viewport_read())
   {
      /* Avoid taking screenshot of GUI overlays. */
      video_driver_set_texture_enable(false, false);
      if (!runloop_ctl(RUNLOOP_CTL_IS_IDLE, NULL))
         video_driver_cached_frame();
#if defined(VITA)
      return take_screenshot_raw(name_base, NULL, savestate);
#else
      return take_screenshot_viewport(name_base, savestate);
#endif
   }

   if (!video_driver_cached_frame_has_valid_framebuffer())
      return take_screenshot_raw(name_base, NULL, savestate);

   if (video_driver_supports_read_frame_raw())
   {
      size_t old_pitch;
      unsigned old_width, old_height;
      bool ret             = false;
      void *frame_data     = NULL;
      const void* old_data = NULL;

      video_driver_cached_frame_get(&old_data, &old_width, &old_height,
            &old_pitch);

      frame_data = video_driver_read_frame_raw(
            &old_width, &old_height, &old_pitch);

      video_driver_cached_frame_set(old_data, old_width, old_height,
            old_pitch);

      if (frame_data)
      {
         video_driver_set_cached_frame_ptr(frame_data);
         if (take_screenshot_raw(name_base, frame_data, savestate))
            ret = true;
      }

      return ret;
   }

   return false;
}

/**
 * take_screenshot:
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool take_screenshot(void)
{
   char *name_base            = strdup(path_get(RARCH_PATH_BASENAME));
   bool            is_paused  = runloop_ctl(RUNLOOP_CTL_IS_PAUSED, NULL);
   bool             ret       = take_screenshot_choice(name_base, false);
   const char *msg_screenshot = ret
      ? msg_hash_to_str(MSG_TAKING_SCREENSHOT)  :
        msg_hash_to_str(MSG_FAILED_TO_TAKE_SCREENSHOT);
   const char *msg            = msg_screenshot;

   free(name_base);

   runloop_msg_queue_push(msg, 1, is_paused ? 1 : 180, true);

   if (is_paused)
   {
      if (!runloop_ctl(RUNLOOP_CTL_IS_IDLE, NULL))
         video_driver_cached_frame();
   }

   return ret;
}

bool take_savestate_screenshot(const char *name_base)
{
   bool            is_paused  = runloop_ctl(RUNLOOP_CTL_IS_PAUSED, NULL);
   bool             ret       = take_screenshot_choice(name_base, true);

   if (is_paused)
   {
      if (!runloop_ctl(RUNLOOP_CTL_IS_IDLE, NULL))
         video_driver_cached_frame();
   }

   return ret;
}

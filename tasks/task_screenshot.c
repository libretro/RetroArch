/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Brad Parker
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
#include "../retroarch.h"
#include "../paths.h"
#include "../msg_hash.h"

#include "../gfx/video_driver.h"

#include "tasks_internal.h"

typedef struct screenshot_task_state screenshot_task_state_t;

struct screenshot_task_state
{
#ifdef _XBOX1
   D3DSurface *surf;
#endif
   char filename[PATH_MAX_LENGTH];
   char shotname[256];
   uint8_t *out_buffer;
   struct scaler_ctx scaler;
   const void *frame;
   unsigned width;
   unsigned height;
   int pitch;
   bool bgr24;
   bool silence;
   void *userbuf;
   bool is_idle;
   bool is_paused;
   bool history_list_enable;
   unsigned pixel_format_type;
};

/**
 * task_screenshot_handler:
 * @task : the task being worked on
 *
 * Saves a screenshot to disk.
 **/
static void task_screenshot_handler(retro_task_t *task)
{
#ifdef HAVE_RBMP
   enum rbmp_source_type bmp_type = RBMP_SOURCE_TYPE_DONT_CARE;
#endif
   screenshot_task_state_t *state = (screenshot_task_state_t*)task->state;
   struct scaler_ctx *scaler      = (struct scaler_ctx*)&state->scaler;
   bool ret                       = false;

   if (task_get_progress(task) == 100)
   {
      task_set_finished(task, true);

      if (state->userbuf)
         free(state->userbuf);

      free(state);
      return;
   }
    
#ifdef HAVE_RBMP
    (void)bmp_type;
#endif

#if defined(_XBOX1)
   if (XGWriteSurfaceToFile(state->surf, state->filename) == S_OK)
      ret = true;
   state->surf->Release();
#elif defined(HAVE_RPNG)
   if (state->bgr24)
      scaler->in_fmt   = SCALER_FMT_BGR24;
   else if (state->pixel_format_type == RETRO_PIXEL_FORMAT_XRGB8888)
      scaler->in_fmt   = SCALER_FMT_ARGB8888;
   else
      scaler->in_fmt   = SCALER_FMT_RGB565;

   video_frame_convert_to_bgr24(
         scaler,
         state->out_buffer,
         (const uint8_t*)state->frame + ((int)state->height - 1) 
         * state->pitch,
         state->width, state->height,
         -state->pitch);

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
   if (state->bgr24)
      bmp_type = RBMP_SOURCE_TYPE_BGR24;
   else if (state->pixel_format_type == RETRO_PIXEL_FORMAT_XRGB8888)
      bmp_type = RBMP_SOURCE_TYPE_XRGB888;

   ret = rbmp_save_image(state->filename,
         state->frame,
         state->width,
         state->height,
         state->pitch,
         bmp_type);
#endif

#ifdef HAVE_IMAGEVIEWER
   if (ret && !state->silence)
   {
      if (
            state->history_list_enable 
            && g_defaults.image_history 
            && playlist_push(
               g_defaults.image_history,
               state->filename,
               NULL,
               "builtin",
               "imageviewer",
               NULL,
               NULL
               )
         )
         playlist_write_file(g_defaults.image_history);
   }
#endif

   task_set_progress(task, 100);

   if (!ret)
   {
      char *msg = strdup(msg_hash_to_str(MSG_FAILED_TO_TAKE_SCREENSHOT));
      runloop_msg_queue_push(msg, 1, state->is_paused ? 1 : 180, true);
      free(msg);
   }
}

/* Take frame bottom-up. */
static bool screenshot_dump(
      const char *name_base,
      const void *frame,
      unsigned width,
      unsigned height,
      int pitch, bool bgr24,
      void *userbuf, bool savestate,
      bool is_idle,
      bool is_paused)
{
   char screenshot_path[PATH_MAX_LENGTH];
   uint8_t *buf                   = NULL;
#ifdef _XBOX1
   d3d_video_t *d3d               = (d3d_video_t*)video_driver_get_ptr(true);
#endif
   settings_t *settings           = config_get_ptr();
   retro_task_t *task             = (retro_task_t*)calloc(1, sizeof(*task));
   screenshot_task_state_t *state = (screenshot_task_state_t*)
         calloc(1, sizeof(*state));
   const char *screenshot_dir     = settings->paths.directory_screenshot;

   screenshot_path[0]             = '\0';

   if (string_is_empty(screenshot_dir) || settings->bools.screenshots_in_content_dir)
   {
      fill_pathname_basedir(screenshot_path, name_base,
            sizeof(screenshot_path));
      screenshot_dir = screenshot_path;
   }

   state->is_idle             = is_idle;
   state->is_paused           = is_paused;
   state->bgr24               = bgr24;
   state->height              = height;
   state->width               = width;
   state->pitch               = pitch;
   state->frame               = frame;
   state->userbuf             = userbuf;
   state->silence             = savestate;
   state->history_list_enable = settings->bools.history_list_enable;
   state->pixel_format_type   = video_driver_get_pixel_format();

   if (savestate)
      snprintf(state->filename,
            sizeof(state->filename), "%s.png", name_base);
   else
   {
      if (settings->bools.auto_screenshot_filename)
         fill_str_dated_filename(state->shotname, path_basename(name_base),
               IMG_EXT, sizeof(state->shotname));
      else
         snprintf(state->shotname, sizeof(state->shotname),
               "%s.png", path_basename(name_base));

      fill_pathname_join(state->filename, screenshot_dir,
            state->shotname, sizeof(state->filename));
   }

#ifdef _XBOX1
   d3d->dev->GetBackBuffer(-1, D3DBACKBUFFER_TYPE_MONO, &state->surf);
#elif defined(HAVE_RPNG)
   buf = (uint8_t*)malloc(width * height * 3);
   if (!buf)
   {
      if (task)
         free(task);
      free(state);
      return false;
   }
   state->out_buffer = buf;
#endif

   task->type        = TASK_TYPE_BLOCKING;
   task->state       = state;
   task->handler     = task_screenshot_handler;

   if (!savestate)
      task->title    = strdup(msg_hash_to_str(MSG_TAKING_SCREENSHOT));

   task_queue_push(task);

   return true;
}

#if !defined(VITA)
static bool take_screenshot_viewport(const char *name_base, bool savestate,
      bool is_idle, bool is_paused)
{
   struct video_viewport vp;
   uint8_t *buffer                       = NULL;
   bool retval                           = false;

   vp.x                                  = 0;
   vp.y                                  = 0;
   vp.width                              = 0;
   vp.height                             = 0;
   vp.full_width                         = 0;
   vp.full_height                        = 0;

   video_driver_get_viewport_info(&vp);

   if (!vp.width || !vp.height)
      return false;

   buffer = (uint8_t*)malloc(vp.width * vp.height * 3);

   if (!buffer)
      return false;

   if (!video_driver_read_viewport(buffer, is_idle))
      goto error;

   /* Data read from viewport is in bottom-up order, suitable for BMP. */
   if (!screenshot_dump(name_base,
            buffer, vp.width, vp.height,
            vp.width * 3, true, buffer, savestate, is_idle, is_paused))
      goto error;

   return true;

error:
   if (buffer)
      free(buffer);
   return retval;
}
#endif

static bool take_screenshot_raw(const char *name_base, void *userbuf,
      bool savestate, bool is_idle, bool is_paused)
{
   size_t pitch;
   unsigned width, height;
   const void *data                      = NULL;

   video_driver_cached_frame_get(&data, &width, &height, &pitch);

   /* Negative pitch is needed as screenshot takes bottom-up,
    * but we use top-down.
    */
   if (!screenshot_dump(name_base, 
         (const uint8_t*)data + (height - 1) * pitch,
         width, height, (int)(-pitch), false, userbuf, savestate, is_idle, is_paused))
      return false;

   return true;
}

static bool take_screenshot_choice(const char *name_base, bool savestate,
      bool is_paused, bool is_idle, bool has_valid_framebuffer)
{
   size_t old_pitch;
   unsigned old_width, old_height;
   bool ret                    = false;
   void *frame_data            = NULL;
   const void* old_data        = NULL;
   settings_t *settings        = config_get_ptr();
   const char *screenshot_dir  = settings->paths.directory_screenshot;

   /* No way to infer screenshot directory. */
   if (     string_is_empty(screenshot_dir)
         && string_is_empty(name_base))
      return false;

   if (video_driver_supports_viewport_read())
   {
      /* Avoid taking screenshot of GUI overlays. */
      video_driver_set_texture_enable(false, false);
      if (!is_idle)
         video_driver_cached_frame();
#if defined(VITA)
      return take_screenshot_raw(name_base, NULL, savestate, is_idle, is_paused);
#else
      return take_screenshot_viewport(name_base, savestate, is_idle, is_paused);
#endif
   }

   if (!has_valid_framebuffer)
      return take_screenshot_raw(name_base, NULL, savestate, is_idle, is_paused);

   if (!video_driver_supports_read_frame_raw())
      return false;

   video_driver_cached_frame_get(&old_data, &old_width, &old_height,
         &old_pitch);

   frame_data = video_driver_read_frame_raw(
         &old_width, &old_height, &old_pitch);

   video_driver_cached_frame_set(old_data, old_width, old_height,
         old_pitch);

   if (frame_data)
   {
      video_driver_set_cached_frame_ptr(frame_data);
      if (take_screenshot_raw(name_base, frame_data, savestate, is_idle, is_paused))
         ret = true;
   }

   return ret;
}

bool take_screenshot(const char *name_base, bool silence, bool has_valid_framebuffer)
{
   bool is_paused         = false;
   bool is_idle           = false;
   bool is_slowmotion     = false;
   bool is_perfcnt_enable = false;
   bool ret               = false;

   runloop_get_status(&is_paused, &is_idle, &is_slowmotion, &is_perfcnt_enable);

   ret       = take_screenshot_choice(name_base, silence, is_paused, is_idle,
         has_valid_framebuffer);

   if (is_paused && !is_idle)
         video_driver_cached_frame();

   return ret;
}

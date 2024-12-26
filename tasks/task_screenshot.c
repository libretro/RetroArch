/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#include <boolean.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <file/file_path.h>
#include <compat/strl.h>
#include <string/stdstring.h>
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

#if defined(HAVE_GFX_WIDGETS)
#include "../gfx/gfx_widgets.h"
#endif

#include "../defaults.h"
#include "../command.h"
#include "../configuration.h"
#include "../core.h"
#include "../gfx/video_driver.h"
#include "../paths.h"
#include "../msg_hash.h"
#include "../runloop.h"

#include "tasks_internal.h"

static bool screenshot_dump_direct(screenshot_task_state_t *state)
{
   struct scaler_ctx *scaler     = (struct scaler_ctx*)&state->scaler;
   bool ret                      = false;

#if defined(HAVE_RPNG)
   if (state->flags & SS_TASK_FLAG_BGR24)
      scaler->in_fmt             = SCALER_FMT_BGR24;
   else if (state->pixel_format_type == RETRO_PIXEL_FORMAT_XRGB8888)
      scaler->in_fmt             = SCALER_FMT_ARGB8888;
   else
      scaler->in_fmt             = SCALER_FMT_RGB565;

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
   {
      enum rbmp_source_type bmp_type = RBMP_SOURCE_TYPE_DONT_CARE;
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
   }
#endif

   return ret;
}

/**
 * task_screenshot_handler:
 * @task : the task being worked on
 *
 * Saves a screenshot to disk.
 **/
static void task_screenshot_handler(retro_task_t *task)
{
   screenshot_task_state_t *state = NULL;
   bool ret                       = false;

   if (!task)
      return;

   if (!(state = (screenshot_task_state_t*)task->state))
      goto task_finished;
   if (task_get_cancelled(task))
      goto task_finished;
   if (task_get_progress(task) == 100)
      goto task_finished;

   /* Take screenshot */
   ret = screenshot_dump_direct(state);

   /* Push screenshot to image history playlist */
#ifdef HAVE_IMAGEVIEWER
   if (       ret
         && !(state->flags & SS_TASK_FLAG_SILENCE)
         &&  (state->flags & SS_TASK_FLAG_HISTORY_LIST_ENABLE)
         )
   {
      struct playlist_entry entry = {0};

      /* the push function reads our entry as const, so these casts are safe */
      entry.path                  = state->filename;
      entry.core_path             = (char*)"builtin";
      entry.core_name             = (char*)"imageviewer";

      command_playlist_push_write(g_defaults.image_history, &entry);
   }
#endif

   task_set_progress(task, 100);

   /* Report any errors */
   if (!ret)
   {
      char *msg = strdup(msg_hash_to_str(MSG_FAILED_TO_TAKE_SCREENSHOT));
      runloop_msg_queue_push(msg, 1, (state->flags & SS_TASK_FLAG_IS_PAUSED) ? 1 : 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      free(msg);
   }

   if (task->title)
      task_free_title(task);

   return;

task_finished:

   task_set_finished(task, true);

   if (task->title)
      task_free_title(task);

   if (state && state->userbuf)
      free(state->userbuf);

#if defined(HAVE_GFX_WIDGETS)
   /* If display widgets are enabled, state is freed
      in the callback after the notification
      is displayed */
   if (state && !(state->flags & SS_TASK_FLAG_WIDGETS_READY))
#endif
   {
      free(state);
      /* Must explicitly set task->state to NULL here,
       * to avoid potential heap-use-after-free errors */
      state       = NULL;
      task->state = NULL;
   }
}

#if defined(HAVE_GFX_WIDGETS)
static void task_screenshot_callback(retro_task_t *task,
      void *task_data,
      void *user_data, const char *error)
{
   screenshot_task_state_t *state = NULL;

   if (!task)
      return;

   if (!(state = (screenshot_task_state_t*)task->state))
      return;

   if (    !(state->flags & SS_TASK_FLAG_SILENCE)
         && (state->flags & SS_TASK_FLAG_WIDGETS_READY))
      gfx_widget_screenshot_taken(dispwidget_get_ptr(),
            state->shotname, state->filename);

   free(state);
   /* Must explicitly set task->state to NULL here,
    * to avoid potential heap-use-after-free errors */
   state       = NULL;
   task->state = NULL;
}
#endif

/* Take frame bottom-up. */
static bool screenshot_dump(
      const char *screenshot_dir,
      const char *name_base,
      const void *frame,
      unsigned width,
      unsigned height,
      int pitch,
      bool bgr24,
      void *userbuf,
      bool savestate,
      uint32_t runloop_flags,
      bool fullpath,
      bool use_thread,
      unsigned pixel_format_type)
{
   uint8_t *buf                   = NULL;
   settings_t *settings           = config_get_ptr();
   bool history_list_enable       = settings->bools.history_list_enable;
   screenshot_task_state_t *state = (screenshot_task_state_t*)
         calloc(1, sizeof(*state));

   if (!state)
      return false;

   /* If fullpath is true, name_base already contains a
    * static path + filename to save the screenshot to. */
   if (fullpath)
      strlcpy(state->filename, name_base, sizeof(state->filename));

   if (runloop_flags & RUNLOOP_FLAG_IDLE)
      state->flags              |= SS_TASK_FLAG_IS_IDLE;
   if (runloop_flags & RUNLOOP_FLAG_PAUSED)
      state->flags              |= SS_TASK_FLAG_IS_PAUSED;
   if (bgr24)
      state->flags              |= SS_TASK_FLAG_BGR24;
   state->height                 = height;
   state->width                  = width;
   state->pitch                  = pitch;
   state->frame                  = frame;
   state->userbuf                = userbuf;
#if defined(HAVE_GFX_WIDGETS)
   if (gfx_widgets_ready())
      state->flags              |= SS_TASK_FLAG_WIDGETS_READY;
#endif
   if (savestate)
      state->flags              |= SS_TASK_FLAG_SILENCE;

   if (history_list_enable)
      state->flags              |= SS_TASK_FLAG_HISTORY_LIST_ENABLE;
   state->pixel_format_type      = pixel_format_type;

   if (!fullpath)
   {
      if (savestate)
      {
         size_t len             = strlcpy(state->filename,
               name_base, sizeof(state->filename));
         strlcpy(state->filename       + len,
               ".png",
               sizeof(state->filename) - len);
      }
      else
      {
         char new_screenshot_dir[PATH_MAX_LENGTH];

         if (!string_is_empty(screenshot_dir))
         {
            const char *content_dir = path_get(RARCH_PATH_BASENAME);

            /* Append content directory name to screenshot
             * path, if required */
            if (settings->bools.sort_screenshots_by_content_enable &&
                !string_is_empty(content_dir))
            {
               char content_dir_name[PATH_MAX_LENGTH];
               fill_pathname_parent_dir_name(content_dir_name,
                     content_dir, sizeof(content_dir_name));
               fill_pathname_join_special(
                     new_screenshot_dir,
                     screenshot_dir,
                     content_dir_name,
                     sizeof(new_screenshot_dir));
            }
            else
               strlcpy(new_screenshot_dir, screenshot_dir,
                     sizeof(new_screenshot_dir));
         }

         if (settings->bools.auto_screenshot_filename)
         {
            const char *screenshot_name = NULL;

            if (path_is_empty(RARCH_PATH_CONTENT))
            {
               struct retro_system_info sysinfo;
               if (!core_get_system_info(&sysinfo))
               {
                  free(state);
                  return false;
               }

               if (string_is_empty(sysinfo.library_name))
                  screenshot_name = "RetroArch";
               else
                  screenshot_name = sysinfo.library_name;
            }
            else
               screenshot_name = path_basename_nocompression(name_base);

            fill_str_dated_filename(state->shotname, screenshot_name,
                  IMG_EXT, sizeof(state->shotname));
         }
         else
         {
            size_t len = strlcpy(state->shotname,
                  path_basename_nocompression(name_base),
                  sizeof(state->shotname));
            strlcpy(state->shotname       + len,
                  ".png",
                  sizeof(state->shotname) - len);
         }

         if (     string_is_empty(new_screenshot_dir)
               || settings->bools.screenshots_in_content_dir)
            fill_pathname_basedir(new_screenshot_dir, name_base,
                  sizeof(new_screenshot_dir));

         fill_pathname_join_special(state->filename, new_screenshot_dir,
               state->shotname, sizeof(state->filename));

         /* Create screenshot directory, if required */
         if (!path_is_directory(new_screenshot_dir))
            if (!path_mkdir(new_screenshot_dir))
               return false;
      }
   }

#if defined(HAVE_RPNG)
   if (!(buf = (uint8_t*)malloc(width * height * 3)))
   {
      free(state);
      return false;
   }
   state->out_buffer     = buf;
#endif

   if (use_thread)
   {
      retro_task_t *task = task_init();

      task->type         = TASK_TYPE_BLOCKING;
      task->state        = state;
      task->handler      = task_screenshot_handler;
      task->mute         = savestate;
#if defined(HAVE_GFX_WIDGETS)
      /* This callback is only required when
       * widgets are enabled */
      if (state->flags & SS_TASK_FLAG_WIDGETS_READY)
         task->callback    = task_screenshot_callback;

      if ((state->flags & SS_TASK_FLAG_WIDGETS_READY) && !savestate)
         task_free_title(task);
      else
#endif
      {
         if (!savestate & settings->bools.notification_show_screenshot)
            task->title = strdup(msg_hash_to_str(MSG_TAKING_SCREENSHOT));
      }

      if (task_queue_push(task))
         return true;

      /* There is already a blocking task going on */
      if (task->title)
         task_free_title(task);

      free(task);

      if (state->out_buffer)
         free(state->out_buffer);

      free(state);

      return false;
   }

   return screenshot_dump_direct(state);
}

static bool take_screenshot_viewport(
      const char *screenshot_dir,
      const char *name_base,
      bool savestate,
      uint32_t runloop_flags,
      bool fullpath,
      bool use_thread,
      unsigned pixel_format_type)
{
   struct video_viewport vp;
   video_driver_state_t *video_st        = video_state_get_ptr();
   uint8_t *buffer                       = NULL;

   vp.x                                  = 0;
   vp.y                                  = 0;
   vp.width                              = 0;
   vp.height                             = 0;
   vp.full_width                         = 0;
   vp.full_height                        = 0;

   video_driver_get_viewport_info(&vp);

   if (!vp.width || !vp.height)
      return false;
   if (!(buffer = (uint8_t*)malloc(vp.width * vp.height * 3)))
      return false;

   if (!(   video_st->current_video->read_viewport
         && video_st->current_video->read_viewport(
            video_st->data, buffer, runloop_flags & RUNLOOP_FLAG_IDLE)))
      goto error;

   /* Data read from viewport is in bottom-up order, suitable for BMP. */
   if (!screenshot_dump(screenshot_dir,
            name_base,
            buffer, vp.width, vp.height,
            vp.width * 3, true, buffer,
            savestate, runloop_flags, fullpath, use_thread,
            pixel_format_type))
      goto error;

   return true;

error:
   free(buffer);
   return false;
}

static bool take_screenshot_raw(
      video_driver_state_t *video_st,
      const char *screenshot_dir,
      const char *name_base, void *userbuf,
      bool savestate, uint32_t runloop_flags, bool fullpath, bool use_thread,
      unsigned pixel_format_type)
{
   const void *data       = video_st->frame_cache_data;
   unsigned width         = video_st->frame_cache_width;
   unsigned height        = video_st->frame_cache_height;
   size_t pitch           = video_st->frame_cache_pitch;
   /* Negative pitch is needed as screenshot takes bottom-up,
    * but we use top-down.
    */
   return screenshot_dump(screenshot_dir,
            name_base,
            (const uint8_t*)data + (height - 1) * pitch,
            width,
            height,
            (int)(-pitch),
            false,
            userbuf,
            savestate,
            runloop_flags,
            fullpath,
            use_thread,
            pixel_format_type);
}

static bool take_screenshot_choice(
      video_driver_state_t *video_st,
      const char *screenshot_dir,
      const char *name_base,
      bool savestate,
      uint32_t runloop_flags,
      bool has_valid_framebuffer,
      bool fullpath,
      bool use_thread,
      bool supports_viewport_read,
      bool supports_read_frame_raw,
      unsigned pixel_format_type
      )
{
   if (supports_viewport_read)
   {
      /* Avoid taking screenshot of GUI overlays. */
      if (video_st->poke && video_st->poke->set_texture_enable)
         video_st->poke->set_texture_enable(video_st->data,
               false, false);
      if (!(runloop_flags & RUNLOOP_FLAG_IDLE))
         video_driver_cached_frame();
      return take_screenshot_viewport(screenshot_dir,
            name_base, savestate, runloop_flags, fullpath, use_thread,
            pixel_format_type);
   }

   if (!has_valid_framebuffer)
      return take_screenshot_raw(video_st, screenshot_dir,
            name_base, NULL, savestate, runloop_flags, fullpath, use_thread,
            pixel_format_type);

   if (supports_read_frame_raw)
   {
      const void *old_data          = video_st->frame_cache_data;
      unsigned old_width            = video_st->frame_cache_width;
      unsigned old_height           = video_st->frame_cache_height;
      size_t old_pitch              = video_st->frame_cache_pitch;
      void *frame_data              = video_driver_read_frame_raw(
            &old_width, &old_height, &old_pitch);

      video_st->frame_cache_data    = old_data;
      video_st->frame_cache_width   = old_width;
      video_st->frame_cache_height  = old_height;
      video_st->frame_cache_pitch   = old_pitch;

      if (frame_data)
      {
         video_st->frame_cache_data = frame_data;
         return take_screenshot_raw(video_st, screenshot_dir,
               name_base, frame_data, savestate, runloop_flags, fullpath, use_thread,
               pixel_format_type);
      }
   }

   return false;
}

bool take_screenshot(
      const char *screenshot_dir,
      const char *name_base,
      bool savestate, bool has_valid_framebuffer,
      bool fullpath, bool use_thread)
{
   bool ret                       = false;
   uint32_t runloop_flags         = runloop_get_flags();
   settings_t *settings           = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   bool video_gpu_screenshot      = settings->bools.video_gpu_screenshot;
   bool supports_viewport_read    = video_st->current_video->read_viewport
         && (video_st->current_video->viewport_info);
   bool prefer_viewport_read      = false;
   if (supports_viewport_read)
   {
      /* Use VP read screenshots if it's a HW context core
       * and read_frame_raw is not implemented */
      if (      video_driver_is_hw_context()
            && !video_st->current_video->read_frame_raw)
         prefer_viewport_read     = true;
      /* Avoid GPU screenshots with savestates */
      if (video_gpu_screenshot && !savestate)
         prefer_viewport_read     = true;
   }

   /* No way to infer screenshot directory. */
   if (     string_is_empty(screenshot_dir)
         && string_is_empty(name_base))
      return false;

   ret       = take_screenshot_choice(
		   video_st,
         screenshot_dir,
         name_base,
         savestate,
         runloop_flags,
         has_valid_framebuffer,
         fullpath,
         use_thread,
         prefer_viewport_read,
         (video_st->current_video->read_frame_raw != NULL),
         video_st->pix_fmt
         );

   if (       (runloop_flags & RUNLOOP_FLAG_PAUSED)
         && (!(runloop_flags & RUNLOOP_FLAG_IDLE)))
         video_driver_cached_frame();

   return ret;
}

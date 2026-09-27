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
#include <gfx/video_frame.h>
#include <streams/file_stream.h>
#include <streams/interface_stream.h>

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

enum screenshot_task_flags
{
   SS_TASK_FLAG_BGR24               = (1 << 0),
   SS_TASK_FLAG_SILENCE             = (1 << 1),
   SS_TASK_FLAG_IS_IDLE             = (1 << 2),
   SS_TASK_FLAG_IS_PAUSED           = (1 << 3),
   SS_TASK_FLAG_HISTORY_LIST_ENABLE = (1 << 4),
   SS_TASK_FLAG_WIDGETS_READY       = (1 << 5),
   SS_TASK_FLAG_HDR                 = (1 << 6)
};

typedef struct screenshot_task_state screenshot_task_state_t;

struct screenshot_task_state
{
   struct scaler_ctx scaler;
   uint8_t *out_buffer;
   const void *frame;
   void *userbuf;

   int pitch;
   unsigned dims;
   unsigned out_dims;
   unsigned pixel_format_type;

   uint8_t flags;

   char filename[PATH_MAX_LENGTH];
   char shotname[NAME_MAX_LENGTH];
   /* Colour-space metadata for an HDR screenshot (SS_TASK_FLAG_HDR). The
    * frame buffer then holds three uint16_t per pixel (48-bit RGB). */
   struct rpng_hdr_metadata hdr;
};

/* The image encoders are pure (bytes in -> bytes out); this task owns
 * all file I/O at the edge.  The preferred shape is encode-to-memory
 * followed by one open/bulk-write/close (filestream_write_file): the
 * compressor is never interleaved with many small VFS writes, the file
 * handle is held for the shortest possible time, and slow media (SD
 * cards, console filesystems) see a single large sequential write.
 * When the in-memory encode cannot allocate (very large frames on
 * memory-constrained platforms), we fall back to streaming the encode
 * straight into the file, which needs only a few rows of scratch. */

#if defined(HAVE_RPNG)
static bool screenshot_save_png(const char *path, const uint8_t *data,
      unsigned dims, signed pitch,
      enum rpng_pixfmt fmt, const struct rpng_hdr_metadata *hdr)
{
   bool ret;
   intfstream_t *intf_s = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   /* Stream the encode straight into the file.  The VFS gives every
    * file a 64 KiB stdio buffer, so the encoder's 16 KiB IDAT chunks
    * already coalesce into large writes; measured at 4K, buffering the
    * whole compressed image first and writing it in one call is
    * indistinguishable in wall time (deflate dominates by two orders
    * of magnitude) while costing a raw-frame-sized allocation that
    * no-overcommit platforms would have to commit up front.  Peak
    * scratch this way is a handful of rows plus the deflate window. */
   if (!intf_s)
      return false;

   ret = rpng_save_image_stream_fmt(data, intf_s,
         VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), pitch, fmt, hdr);

   intfstream_close(intf_s);
   free(intf_s);
   return ret;
}
#elif defined(HAVE_RBMP)
static bool screenshot_save_bmp(const char *path, const void *frame,
      unsigned dims, unsigned pitch,
      enum rbmp_source_type type)
{
   bool ret;
   intfstream_t *intf_s = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   /* BMP has no compression - the file is the raw pixel block - so
    * buffering the whole image first would only add a full-frame
    * allocation and an extra copy of every byte.  The stream encoder
    * writes the pixel block in a single call straight from the source
    * when the stride matches the padded row size (the common case),
    * and per-row through the VFS's 64 KiB stdio buffer otherwise. */
   if (!intf_s)
      return false;

   ret = rbmp_save_image_stream(intf_s, frame,
         VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), pitch, type);

   intfstream_close(intf_s);
   free(intf_s);
   return ret;
}
#endif

static bool screenshot_dump_direct(screenshot_task_state_t *state)
{
   bool ret                      = false;

#if defined(HAVE_RPNG)
   struct scaler_ctx *scaler     = (struct scaler_ctx*)&state->scaler;
   const uint8_t* input          = (const uint8_t*)state->frame
      + ((int)VIDEO_SCALE_H(state->dims) - 1) * state->pitch;

   if (!input)
      return ret;

   /* HDR screenshot: the frame is 48-bit RGB (three uint16_t per pixel),
    * bottom-up, and carries colour-space metadata. Encode a 16-bit PNG
    * tagged with the HDR chunks, using the same negative-pitch top-down
    * trick as the BGR24 fast path. Never resampled. */
   if (state->flags & SS_TASK_FLAG_HDR)
   {
      ret = screenshot_save_png(
            state->filename,
            input,
            state->out_dims,
            -state->pitch,
            RPNG_PIXFMT_RGB48,
            &state->hdr);
      if (state->out_buffer)
         free(state->out_buffer);
      return ret;
   }

   /* Fast path: source is already BGR24 and no resampling is
    * needed, so hand the source buffer directly to the PNG
    * encoder. rpng_save_image_stream walks rows via `data +=
    * pitch` with a signed pitch, so a bottom-up source is
    * encoded top-down for free by starting at the last row and
    * passing a negative row stride (same trick take_screenshot_raw
    * uses via screenshot_dump's pitch argument).
    *
    * This avoids allocating a second full-frame BGR24 buffer and
    * the flip-and-copy the scaler would otherwise do between them;
    * at 4K that is ~48 MiB of allocation and copy per screenshot. */
   if (     (state->flags & SS_TASK_FLAG_BGR24)
         &&  state->out_dims == state->dims)
   {
      ret = screenshot_save_png(
            state->filename,
            input,
            state->out_dims,
            -state->pitch,
            RPNG_PIXFMT_BGR24,
            NULL);
      /* state->out_buffer is NULL in this path (see screenshot_dump);
       * nothing to free. */
      return ret;
   }

   /* Same idea for the raw-framebuffer formats: when no resampling is
    * needed, feed the core's XRGB8888/RGB565 rows straight to the
    * encoder (which converts per row) instead of converting the whole
    * frame into a BGR24 buffer first - that is a width*height*3
    * allocation plus a full extra pass over every pixel (~24 MiB at
    * 4K), for output that is pixel-identical. */
   if (     !(state->flags & SS_TASK_FLAG_BGR24)
         &&  state->out_dims == state->dims)
   {
      ret = screenshot_save_png(
            state->filename,
            input,
            state->out_dims,
            -state->pitch,
            (state->pixel_format_type == RETRO_PIXEL_FORMAT_XRGB8888)
                  ? RPNG_PIXFMT_XRGB8888
                  : RPNG_PIXFMT_RGB565,
            NULL);
      /* state->out_buffer is NULL in this path (see screenshot_dump). */
      return ret;
   }

   if (state->flags & SS_TASK_FLAG_BGR24)
      scaler->in_fmt             = SCALER_FMT_BGR24;
   else if (state->pixel_format_type == RETRO_PIXEL_FORMAT_XRGB8888)
      scaler->in_fmt             = SCALER_FMT_ARGB8888;
   else
      scaler->in_fmt             = SCALER_FMT_RGB565;

   video_frame_convert_to_bgr24(
         scaler,
         state->out_buffer,
         input,
         VIDEO_SCALE_W(state->dims),
         VIDEO_SCALE_H(state->dims),
         -state->pitch,
         VIDEO_SCALE_W(state->out_dims),
         VIDEO_SCALE_H(state->out_dims),
         VIDEO_SCALE_W(state->out_dims) * 3
         );

   scaler_ctx_gen_reset(&state->scaler);

   ret = screenshot_save_png(
         state->filename,
         state->out_buffer,
         state->out_dims,
         (signed)(VIDEO_SCALE_W(state->out_dims) * 3),
         RPNG_PIXFMT_BGR24,
         NULL);

   free(state->out_buffer);
#elif defined(HAVE_RBMP)
   {
      enum rbmp_source_type bmp_type = RBMP_SOURCE_TYPE_DONT_CARE;
      if (state->flags & SS_TASK_FLAG_BGR24)
         bmp_type = RBMP_SOURCE_TYPE_BGR24;
      else if (state->pixel_format_type == RETRO_PIXEL_FORMAT_XRGB8888)
         bmp_type = RBMP_SOURCE_TYPE_XRGB888;

      ret = screenshot_save_bmp(state->filename,
            state->frame,
            state->dims,
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
   uint8_t flg;
   screenshot_task_state_t *state = NULL;
   bool ret                       = false;

   if (!task)
      return;

   if (!(state = (screenshot_task_state_t*)task->state))
      goto task_finished;

   flg = task_get_flags(task);

   if ((flg & RETRO_TASK_FLG_CANCELLED) > 0)
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
      const char *_msg = msg_hash_to_str(MSG_FAILED_TO_TAKE_SCREENSHOT);
      runloop_msg_queue_push(_msg, strlen(_msg), 1,
            (state->flags & SS_TASK_FLAG_IS_PAUSED) ? 1 : 180, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);

#if defined(HAVE_GFX_WIDGETS)
      /* Do not show empty widget success on error */
      if (state)
         state->flags |= SS_TASK_FLAG_SILENCE;
#endif
   }

   if (task->title)
      task_free_title(task);

   return;

task_finished:
   if (task)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);

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

/**
 * screenshot_rotate:
 *
 * Copies rotated @source image to @target
 * according to @rotate_type 'enum video_rotation_type'.
 **/
static void screenshot_rotate(
      uint32_t *target,
      uint32_t *source,
      size_t size,
      unsigned *dims,
      size_t *pitch,
      uint8_t rotate_type)
{
   int y;
   size_t x;
   size_t bpp          = *pitch / VIDEO_SCALE_W(*dims);
   size_t source_pitch = *pitch;
   int source_width    = VIDEO_SCALE_W(*dims);
   int source_height   = VIDEO_SCALE_H(*dims);
   int target_width    = source_width;
   int target_height   = source_height;
   size_t target_pitch = source_pitch;

   /* 90 deg dimension flip */
   if (     rotate_type == VIDEO_ROTATION_90_DEG
         || rotate_type == VIDEO_ROTATION_270_DEG)
   {
      target_width     = source_height;
      target_height    = source_width;
      target_pitch     = target_width * bpp;
   }

   for (y = 0; y < target_height; y++)
   {
      for (x = 0; x < target_pitch; x++)
      {
         size_t pixel_source = (y * source_width) + x;
         size_t pixel_target = (y * target_width) + x;

         switch (rotate_type)
         {
            default:
            case VIDEO_ROTATION_NORMAL:
               break;
            case VIDEO_ROTATION_90_DEG:
               pixel_source = ((target_height - 1 - y) + (x * source_width));
               break;
            case VIDEO_ROTATION_180_DEG:
               pixel_source = ((target_height - 1 - y) * target_width) + (target_width - 1 - x);
               break;
            case VIDEO_ROTATION_270_DEG:
               pixel_source = y + ((target_width - 1 - x) * source_width);
               break;
         }

         if (     pixel_source < 0
               || pixel_target < 0
               || pixel_source > size / bpp
               || pixel_target > size / bpp)
            continue;

         *(target + pixel_target) = *(source + pixel_source);
      }
   }

   /* Replace source values with target values */
   *dims   = VIDEO_SCALE_PACK(target_width, target_height);
   *pitch  = target_pitch;
}

/* Take frame bottom-up. */
static bool screenshot_dump(
      const char *screenshot_dir,
      const char *name_base,
      const void *frame,
      unsigned dims,
      int pitch,
      bool bgr24,
      void *userbuf,
      bool savestate,
      uint32_t runloop_flags,
      bool fullpath,
      bool use_thread,
      unsigned pixel_format_type,
      const struct rpng_hdr_metadata *hdr)
{
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
   if (hdr)
   {
      state->flags              |= SS_TASK_FLAG_HDR;
      state->hdr                 = *hdr;
   }
   state->dims                   = dims;
   state->out_dims               = dims;
   state->pitch                  = pitch;
   state->frame                  = frame;
   state->userbuf                = userbuf;
#if defined(HAVE_GFX_WIDGETS)
   if (gfx_widgets_ready())
      state->flags              |= SS_TASK_FLAG_WIDGETS_READY;
#endif
   if (savestate)
   {
      /* Use native core output dimensions */
      unsigned cache_dims = 0;
      video_driver_state_t *video_st = video_state_get_ptr();
      video_driver_cached_frame_info(&cache_dims, NULL, NULL);
      if (video_st)
      {
         state->out_dims         = VIDEO_SCALE_PACK(
               (VIDEO_SCALE_W(cache_dims) <= 4)
               ? video_st->av_info.geometry.base_width
               : VIDEO_SCALE_W(cache_dims),
               (VIDEO_SCALE_H(cache_dims) <= 4)
               ? video_st->av_info.geometry.base_height
               : VIDEO_SCALE_H(cache_dims));
      }

      /* Fallback to display size if smaller than core output */
      if (     VIDEO_SCALE_W(state->out_dims) > VIDEO_SCALE_W(dims)
            || VIDEO_SCALE_H(state->out_dims) > VIDEO_SCALE_H(dims))
         state->out_dims         = dims;

      state->flags              |= SS_TASK_FLAG_SILENCE;
   }

   if (history_list_enable)
      state->flags              |= SS_TASK_FLAG_HISTORY_LIST_ENABLE;
   state->pixel_format_type      = pixel_format_type;

   if (!fullpath)
   {
      if (savestate)
      {
         size_t _len             = strlcpy(state->filename,
               name_base, sizeof(state->filename));
         strlcpy_lit(state->filename       + _len,
               ".png",
               sizeof(state->filename) - _len);
      }
      else
      {
         char new_screenshot_dir[DIR_MAX_LENGTH];

         if (screenshot_dir && *screenshot_dir)
         {
            const char *content_dir = path_get(RARCH_PATH_BASENAME);

            /* Append content directory name to screenshot
             * path, if required */
            if (    settings->bools.sort_screenshots_by_content_enable
                && content_dir && *content_dir)
            {
               char content_dir_name[DIR_MAX_LENGTH];
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

               if (!sysinfo.library_name || !*sysinfo.library_name)
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
            size_t _len = strlcpy(state->shotname,
                path_basename_nocompression(name_base),
                 sizeof(state->shotname));
            strlcpy_lit(state->shotname       + _len,
                  ".png",
                  sizeof(state->shotname) - _len);
         }

         if (     !*new_screenshot_dir
               || settings->bools.screenshots_in_content_dir)
            fill_pathname_basedir(new_screenshot_dir, name_base,
                  sizeof(new_screenshot_dir));

         fill_pathname_join_special(state->filename, new_screenshot_dir,
               state->shotname, sizeof(state->filename));

         /* Create screenshot directory, if required */
         if (!path_is_directory(new_screenshot_dir))
            if (!path_mkdir(new_screenshot_dir))
            {
               free(state);
               return false;
            }
      }
   }

#if defined(HAVE_RPNG)
   /* Only allocate the BGR24 output buffer when screenshot_dump_direct
    * will actually use the scaler, i.e. when the output dimensions
    * differ from the source.  At matching dimensions every source
    * format (BGR24 viewport read-backs, XRGB8888 and RGB565 core
    * framebuffers, HDR) is encoded directly with a negative pitch and
    * no intermediate buffer is needed. */
   if (   !(state->flags & SS_TASK_FLAG_HDR)
       && state->out_dims != dims)
   {
      if (!(state->out_buffer = (uint8_t*)malloc(
            VIDEO_SCALE_AREA(state->out_dims) * 3)))
      {
         free(state);
         return false;
      }
   }
#endif

   if (use_thread)
   {
      retro_task_t *task = task_init();

      task->type         = TASK_TYPE_BLOCKING;
      task->state        = state;
      task->handler      = task_screenshot_handler;
      if (savestate)
         task->flags    |=  RETRO_TASK_FLG_MUTE;
      else
         task->flags    &= ~RETRO_TASK_FLG_MUTE;
#if defined(HAVE_GFX_WIDGETS)
      /* This callback is only required when
       * widgets are enabled */
      if (state->flags & SS_TASK_FLAG_WIDGETS_READY)
         task->callback  = task_screenshot_callback;

      if ((state->flags & SS_TASK_FLAG_WIDGETS_READY) && !savestate)
         task_free_title(task);
      else
#endif
      {
         if (!savestate && settings->bools.notification_show_screenshot)
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

   {
      /* Same ownership as the task path: the caller's buffer is ours
       * once the screenshot is written, and stays the caller's to
       * free if it is not. */
      bool ret = screenshot_dump_direct(state);
      if (ret && state->userbuf)
         free(state->userbuf);
      free(state);
      return ret;
   }
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
   unsigned output_size;
   video_driver_state_t *video_st = video_state_get_ptr();
   uint8_t *buffer                = NULL;

   vp.pos                         = VIDEO_POS_PACK(0, 0);
   vp.dims                        = 0;
   vp.full_dims                   = 0;

   video_driver_get_viewport_info(&vp);

   if (!VIDEO_SCALE_W(vp.dims) || !VIDEO_SCALE_H(vp.dims))
      return false;

   /* Prefer a native HDR read-back when the driver offers one. This path is
    * entirely additive: if read_viewport_hdr is absent or returns false
    * (e.g. not in HDR mode, or the driver has no HDR read-back), we fall
    * straight through to the ordinary 8-bit SDR read_viewport below, so
    * normal screenshots are unaffected. */
   if (video_st->current_video->read_viewport_hdr)
   {
      struct rpng_hdr_metadata hdr;
      uint16_t *hdr_buffer = (uint16_t*)malloc(VIDEO_SCALE_AREA(vp.dims) * 6);

      memset(&hdr, 0, sizeof(hdr));
      if (hdr_buffer)
      {
         if (video_st->current_video->read_viewport_hdr(
                  video_st->data, hdr_buffer,
                  runloop_flags & RUNLOOP_FLAG_IDLE, &hdr))
         {
            output_size = VIDEO_DRIVER_OUTPUT_DIMS(video_st);
            if (VIDEO_SCALE_W(vp.dims) > VIDEO_SCALE_W(output_size))
               VIDEO_SCALE_PUT_W(vp.dims, VIDEO_SCALE_W(output_size));
            if (VIDEO_SCALE_H(vp.dims) > VIDEO_SCALE_H(output_size))
               VIDEO_SCALE_PUT_H(vp.dims, VIDEO_SCALE_H(output_size));

            /* 48-bit RGB, bottom-up (pitch = width*6, negated top-down
             * inside screenshot_dump_direct like the BGR24 path). */
            if (screenshot_dump(screenshot_dir,
                     name_base,
                     hdr_buffer, vp.dims,
                     VIDEO_SCALE_W(vp.dims) * 6, false, hdr_buffer,
                     savestate, runloop_flags, fullpath, use_thread,
                     pixel_format_type, &hdr))
               return true;
         }
         free(hdr_buffer);
      }
   }

   if (!(buffer = (uint8_t*)malloc(VIDEO_SCALE_AREA(vp.dims) * 3)))
      return false;

   if ((   video_st->current_video->read_viewport
         && video_st->current_video->read_viewport(
            video_st->data, buffer, runloop_flags & RUNLOOP_FLAG_IDLE)))
   {
      /* Limit image to screen size */
      output_size = VIDEO_DRIVER_OUTPUT_DIMS(video_st);
      if (VIDEO_SCALE_W(vp.dims) > VIDEO_SCALE_W(output_size))
         VIDEO_SCALE_PUT_W(vp.dims, VIDEO_SCALE_W(output_size));
      if (VIDEO_SCALE_H(vp.dims) > VIDEO_SCALE_H(output_size))
         VIDEO_SCALE_PUT_H(vp.dims, VIDEO_SCALE_H(output_size));

      /* Data read from viewport is in bottom-up order, suitable for BMP. */
      if (screenshot_dump(screenshot_dir,
               name_base,
               buffer, vp.dims,
               VIDEO_SCALE_W(vp.dims) * 3, true, buffer,
               savestate, runloop_flags, fullpath, use_thread,
               pixel_format_type, NULL))
         return true;
   }

   free(buffer);
   return false;
}

/* Read-side callback for take_screenshot_raw: allocate a copy of
 * the cached frame and stash it in this small POD so the caller
 * gets a stable pointer it owns.  The copy is necessary because
 * the screenshot task may run on a worker thread and the
 * cached_frame's lifetime can end (core close, driver reinit) at
 * any point after the callback returns.
 */
struct ss_raw_copy
{
   void    *buffer;
   unsigned dims;
   size_t   pitch;
};

static void ss_raw_copy_cb(void *userdata,
      const void *data,
      unsigned dims, size_t pitch)
{
   struct ss_raw_copy *out = (struct ss_raw_copy*)userdata;
   size_t             size;

   if (!data || !VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims) || !pitch)
      return;

   size = (size_t)VIDEO_SCALE_H(dims) * pitch;
   if (!(out->buffer = malloc(size)))
      return;

   memcpy(out->buffer, data, size);
   out->dims   = dims;
   out->pitch  = pitch;
}

static bool take_screenshot_raw(
      video_driver_state_t *video_st,
      const char *screenshot_dir,
      const char *name_base,
      bool savestate, uint32_t runloop_flags,
      bool fullpath, bool use_thread,
      unsigned pixel_format_type)
{
   /* Pull a heap-owned copy of the cached frame's pixels via the
    * lifetime-safe callback API.  The screenshot task is deferred
    * onto a worker thread; without copying we'd risk a UAF if the
    * core closes or the driver reinit's between this enqueue and the
    * worker dequeuing it. */
   struct ss_raw_copy copy = { NULL, 0, 0 };
   video_driver_cached_frame_read(&copy, ss_raw_copy_cb);

   if (     !copy.buffer || !VIDEO_SCALE_W(copy.dims)
         || !VIDEO_SCALE_H(copy.dims) || !copy.pitch)
   {
      free(copy.buffer);
      return false;
   }

   /* Rotate the buffer according to core SET_ROTATION */
   {
      runloop_state_t *runloop_st   = runloop_state_get_ptr();
      rarch_system_info_t *sys_info = &runloop_st->system;

      if (sys_info && sys_info->rotation)
      {
         uint32_t *buf = NULL;
         uint8_t bpp   = copy.pitch / VIDEO_SCALE_W(copy.dims);
         size_t size   = VIDEO_SCALE_AREA(copy.dims) * bpp;

         if ((buf = (uint32_t*)calloc(1, size)))
         {
            screenshot_rotate(buf, (uint32_t*)copy.buffer, size,
                  &copy.dims, &copy.pitch,
                  sys_info->rotation);
            memcpy(copy.buffer, buf, size);

            free(buf);
            buf = NULL;
         }
      }
   }

   /* Negative pitch is needed as screenshot takes bottom-up, but
    * we use top-down. */
   if (screenshot_dump(screenshot_dir,
            name_base,
            (const uint8_t*)copy.buffer
            + (VIDEO_SCALE_H(copy.dims) - 1) * copy.pitch,
            copy.dims,
            (int)(-copy.pitch),
            false,
            copy.buffer, /* the task frees it once written */
            savestate,
            runloop_flags,
            fullpath,
            use_thread,
            pixel_format_type,
            NULL))
      return true;

   /* screenshot_dump only takes ownership on success; on failure
    * the copy is still ours to free. */
   free(copy.buffer);
   return false;
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
      bool supports_vp_read,
      unsigned pixel_format_type
      )
{
   if (supports_vp_read)
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
            name_base, savestate, runloop_flags, fullpath, use_thread,
            pixel_format_type);

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
   bool supports_vp_read          = video_st->current_video->read_viewport
         && (video_st->current_video->viewport_info);
   bool prefer_vp_read            = false;
   if (supports_vp_read)
   {
      /* Use VP read screenshots if it's a HW context core */
      if (video_driver_is_hw_context())
         prefer_vp_read           = true;
      /* Avoid GPU screenshots with savestates */
      if (video_gpu_screenshot && !savestate)
         prefer_vp_read           = true;
   }
   /* No way to infer screenshot directory. */
   if (     (!screenshot_dir || !*screenshot_dir)
         && (!name_base || !*name_base))
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
         prefer_vp_read,
         video_st->pix_fmt
         );
   if (       (runloop_flags & RUNLOOP_FLAG_PAUSED)
         && (!(runloop_flags & RUNLOOP_FLAG_IDLE)))
         video_driver_cached_frame();
   return ret;
}


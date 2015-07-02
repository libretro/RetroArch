/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <retro_log.h>
#include <file/file_path.h>
#include <compat/strl.h>

#if defined(HAVE_ZLIB_DEFLATE) && defined(HAVE_RPNG)
#include <formats/rpng.h>
#endif

#include "general.h"
#include "msg_hash.h"
#include "gfx/scaler/scaler.h"
#include "retroarch.h"
#include "runloop.h"
#include "screenshot.h"
#include "gfx/video_driver.h"
#include "gfx/video_viewport.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(HAVE_ZLIB_DEFLATE) && defined(HAVE_RPNG)
#define IMG_EXT "png"
#else
#define IMG_EXT "bmp"

static bool write_header_bmp(FILE *file, unsigned width, unsigned height)
{
   unsigned line_size  = (width * 3 + 3) & ~3;
   unsigned size       = line_size * height + 54;
   unsigned size_array = line_size * height;

   /* Generic BMP stuff. */
   const uint8_t header[] = {
      'B', 'M',
      (uint8_t)(size >> 0), (uint8_t)(size >> 8),
      (uint8_t)(size >> 16), (uint8_t)(size >> 24),
      0, 0, 0, 0,
      54, 0, 0, 0,
      40, 0, 0, 0,
      (uint8_t)(width >> 0), (uint8_t)(width >> 8),
      (uint8_t)(width >> 16), (uint8_t)(width >> 24),
      (uint8_t)(height >> 0), (uint8_t)(height >> 8),
      (uint8_t)(height >> 16), (uint8_t)(height >> 24),
      1, 0,
      24, 0,
      0, 0, 0, 0,
      (uint8_t)(size_array >> 0), (uint8_t)(size_array >> 8),
      (uint8_t)(size_array >> 16), (uint8_t)(size_array >> 24),
      19, 11, 0, 0,
      19, 11, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0
   };

   return fwrite(header, 1, sizeof(header), file) == sizeof(header);
}

static void dump_lines_file(FILE *file, uint8_t **lines,
      size_t line_size, unsigned height)
{
   unsigned i;

   for (i = 0; i < height; i++)
      fwrite(lines[i], 1, line_size, file);
}

static void dump_line_bgr(uint8_t *line, const uint8_t *src, unsigned width)
{
   memcpy(line, src, width * 3);
}

static void dump_line_16(uint8_t *line, const uint16_t *src, unsigned width)
{
   unsigned i;

   for (i = 0; i < width; i++)
   {
      uint16_t pixel = *src++;
      uint8_t b = (pixel >>  0) & 0x1f;
      uint8_t g = (pixel >>  5) & 0x3f;
      uint8_t r = (pixel >> 11) & 0x1f;
      *line++   = (b << 3) | (b >> 2);
      *line++   = (g << 2) | (g >> 4);
      *line++   = (r << 3) | (r >> 2);
   }
}

static void dump_line_32(uint8_t *line, const uint32_t *src, unsigned width)
{
   unsigned i;

   for (i = 0; i < width; i++)
   {
      uint32_t pixel = *src++;
      *line++ = (pixel >>  0) & 0xff;
      *line++ = (pixel >>  8) & 0xff;
      *line++ = (pixel >> 16) & 0xff;
   }
}

static void dump_content(FILE *file, const void *frame,
      int width, int height, int pitch, bool bgr24)
{
   size_t line_size;
   int i, j;
   union
   {
      const uint8_t *u8;
      const uint16_t *u16;
      const uint32_t *u32;
   } u;
   uint8_t **lines = (uint8_t**)calloc(height, sizeof(uint8_t*));

   if (!lines)
      return;

   u.u8      = (const uint8_t*)frame;
   line_size = (width * 3 + 3) & ~3;

   for (i = 0; i < height; i++)
   {
      lines[i] = (uint8_t*)calloc(1, line_size);
      if (!lines[i])
         goto end;
   }

   if (bgr24) /* BGR24 byte order. Can directly copy. */
   {
      for (j = 0; j < height; j++, u.u8 += pitch)
         dump_line_bgr(lines[j], u.u8, width);
   }
   else if (video_driver_get_pixel_format() == RETRO_PIXEL_FORMAT_XRGB8888)
   {
      for (j = 0; j < height; j++, u.u8 += pitch)
         dump_line_32(lines[j], u.u32, width);
   }
   else /* RGB565 */
   {
      for (j = 0; j < height; j++, u.u8 += pitch)
         dump_line_16(lines[j], u.u16, width);
   }

   dump_lines_file(file, lines, line_size, height);

end:
   for (i = 0; i < height; i++)
      free(lines[i]);
   free(lines);
}
#endif

static bool take_screenshot_viewport(void)
{
   char screenshot_path[PATH_MAX_LENGTH] = {0};
   const char *screenshot_dir            = NULL;
   uint8_t *buffer                       = NULL;
   bool retval                           = false;
   struct video_viewport vp              = {0};
   settings_t *settings                  = config_get_ptr();
   global_t *global                      = global_get_ptr();

   video_driver_viewport_info(&vp);

   if (!vp.width || !vp.height)
      return false;

   if (!(buffer = (uint8_t*)malloc(vp.width * vp.height * 3)))
      return false;

   if (!video_driver_read_viewport(buffer))
      goto done;

   screenshot_dir = settings->screenshot_directory;

   if (!*settings->screenshot_directory)
   {
      fill_pathname_basedir(screenshot_path, global->basename,
            sizeof(screenshot_path));
      screenshot_dir = screenshot_path;
   }

   /* Data read from viewport is in bottom-up order, suitable for BMP. */
   if (!screenshot_dump(screenshot_dir, buffer, vp.width, vp.height,
            vp.width * 3, true))
      goto done;

   retval = true;

done:
   if (buffer)
      free(buffer);
   return retval;
}

static bool take_screenshot_raw(void)
{
   unsigned width, height;
   size_t pitch;
   char screenshot_path[PATH_MAX_LENGTH] = {0};
   const void *data                      = NULL;
   const char *screenshot_dir            = NULL;
   global_t *global                      = global_get_ptr();
   settings_t *settings                  = config_get_ptr();

   video_driver_cached_frame_get(&data, &width, &height, &pitch);
   
   screenshot_dir = settings->screenshot_directory;

   if (!*settings->screenshot_directory)
   {
      fill_pathname_basedir(screenshot_path, global->basename,
            sizeof(screenshot_path));
      screenshot_dir = screenshot_path;
   }

   /* Negative pitch is needed as screenshot takes bottom-up,
    * but we use top-down.
    */
   return screenshot_dump(screenshot_dir,
         (const uint8_t*)data + (height - 1) * pitch,
         width, height, -pitch, false);
}

/**
 * take_screenshot:
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool take_screenshot(void)
{
   bool viewport_read   = false;
   bool ret             = true;
   const char *msg      = NULL;
   runloop_t *runloop   = rarch_main_get_ptr();
   driver_t *driver     = driver_get_ptr();
   settings_t *settings = config_get_ptr();
   global_t *global     = global_get_ptr();
   const struct retro_hw_render_callback *hw_render = 
      (const struct retro_hw_render_callback*)video_driver_callback();

   /* No way to infer screenshot directory. */
   if ((!*settings->screenshot_directory) && (!*global->basename))
      return false;

   viewport_read = (settings->video.gpu_screenshot ||
         ((hw_render->context_type
         != RETRO_HW_CONTEXT_NONE) && !driver->video->read_frame_raw))
         && driver->video->read_viewport && driver->video->viewport_info;

   if (viewport_read)
   {
      /* Avoid taking screenshot of GUI overlays. */
      video_driver_set_texture_enable(false, false);

      if (driver->video)
         video_driver_cached_frame();
   }

   if (viewport_read)
      ret = take_screenshot_viewport();
   else if (!video_driver_cached_frame_has_valid_fb())
      ret = take_screenshot_raw();
   else if (driver->video->read_frame_raw)
   {
      unsigned old_width, old_height;
      size_t old_pitch;
      void *frame_data;
      const void* old_data = NULL;
      
      video_driver_cached_frame_get(&old_data, &old_width, &old_height,
            &old_pitch);
      
      frame_data = video_driver_read_frame_raw(
            &old_width, &old_height, &old_pitch);

      video_driver_cached_frame_set(old_data, old_width, old_height,
            old_pitch);

      if (frame_data)
      {
         video_driver_cached_frame_set_ptr(frame_data);
         ret = take_screenshot_raw();
         free(frame_data);
      }
      else
         ret = false;
   }
   else
   {
      RARCH_ERR("%s.\n", msg_hash_to_str(MSG_FAILED_TO_TAKE_SCREENSHOT));
      ret = false;
   }


   if (ret)
   {
      RARCH_LOG("%s.\n", msg_hash_to_str(MSG_TAKING_SCREENSHOT));
      msg = msg_hash_to_str(MSG_TAKING_SCREENSHOT);
   }
   else
   {
      RARCH_WARN("%s.\n", msg_hash_to_str(MSG_FAILED_TO_TAKE_SCREENSHOT));
      msg = msg_hash_to_str(MSG_FAILED_TO_TAKE_SCREENSHOT);
   }

   rarch_main_msg_queue_push(msg, 1, runloop->is_paused ? 1 : 180, true);

   if (runloop->is_paused)
      video_driver_cached_frame();

   return ret;
}


/* Take frame bottom-up. */
bool screenshot_dump(const char *folder, const void *frame,
      unsigned width, unsigned height, int pitch, bool bgr24)
{
   char filename[PATH_MAX_LENGTH] = {0};
   char shotname[PATH_MAX_LENGTH] = {0};
   struct scaler_ctx scaler       = {0};
   FILE *file                     = NULL;
   uint8_t *out_buffer            = NULL;
   bool ret                       = false;
   driver_t *driver               = driver_get_ptr();

   (void)file;
   (void)out_buffer;
   (void)scaler;
   (void)driver;

   fill_dated_filename(shotname, IMG_EXT, sizeof(shotname));
   fill_pathname_join(filename, folder, shotname, sizeof(filename));

#ifdef _XBOX1
   d3d_video_t *d3d = (d3d_video_t*)driver->video_data;
   settings_t *settings = config_get_ptr();

   D3DSurface *surf = NULL;
   d3d->dev->GetBackBuffer(-1, D3DBACKBUFFER_TYPE_MONO, &surf);
   ret = XGWriteSurfaceToFile(surf, filename);
   surf->Release();

   if(ret == S_OK)
   {
      RARCH_LOG("Screenshot saved: %s.\n", filename);
      rarch_main_msg_queue_push("Screenshot saved.", 1, 30, false);
      return true;
   }

   ret = false;
#elif defined(HAVE_ZLIB_DEFLATE) && defined(HAVE_RPNG)
   out_buffer = (uint8_t*)malloc(width * height * 3);
   if (!out_buffer)
      return false;

   scaler.in_width    = width;
   scaler.in_height   = height;
   scaler.out_width   = width;
   scaler.out_height  = height;
   scaler.in_stride   = -pitch;
   scaler.out_stride  = width * 3;
   scaler.out_fmt     = SCALER_FMT_BGR24;
   scaler.scaler_type = SCALER_TYPE_POINT;

   if (bgr24)
      scaler.in_fmt = SCALER_FMT_BGR24;
   else if (video_driver_get_pixel_format() == RETRO_PIXEL_FORMAT_XRGB8888)
      scaler.in_fmt = SCALER_FMT_ARGB8888;
   else
      scaler.in_fmt = SCALER_FMT_RGB565;

   scaler_ctx_gen_filter(&scaler);
   scaler_ctx_scale(&scaler, out_buffer,
         (const uint8_t*)frame + ((int)height - 1) * pitch);
   scaler_ctx_gen_reset(&scaler);

   RARCH_LOG("Using RPNG for PNG screenshots.\n");
   ret = rpng_save_image_bgr24(filename,
         out_buffer, width, height, width * 3);
   if (!ret)
      RARCH_ERR("Failed to take screenshot.\n");
   free(out_buffer);
#else
   file = fopen(filename, "wb");
   if (!file)
   {
      RARCH_ERR("Failed to open file \"%s\" for screenshot.\n", filename);
      return false;
   }

   ret = write_header_bmp(file, width, height);

   if (ret)
      dump_content(file, frame, width, height, pitch, bgr24);
   else
      RARCH_ERR("Failed to write image header.\n");

   fclose(file);
#endif

   return ret;
}


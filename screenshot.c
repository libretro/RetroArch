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

#include <compat/strl.h>
#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include <boolean.h>
#include <stdint.h>
#include <string.h>
#include "general.h"
#include "intl/intl.h"
#include <file/file_path.h>
#include "gfx/scaler/scaler.h"
#include "retroarch.h"
#include "runloop.h"
#include "retroarch_logger.h"
#include "screenshot.h"
#include "gfx/video_viewport.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_ZLIB_DEFLATE

#include <formats/rpng.h>
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
   uint8_t **lines;
   union
   {
      const uint8_t *u8;
      const uint16_t *u16;
      const uint32_t *u32;
   } u;
   u.u8 = (const uint8_t*)frame;

   lines = (uint8_t**)calloc(height, sizeof(uint8_t*));
   if (!lines)
      return;

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
   else if (g_extern.system.pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888)
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
   char screenshot_path[PATH_MAX_LENGTH];
   const char *screenshot_dir = NULL;
   uint8_t *buffer = NULL;
   bool retval = false;
   struct video_viewport vp = {0};

   if (driver.video && driver.video->viewport_info)
      driver.video->viewport_info(driver.video_data, &vp);

   if (!vp.width || !vp.height)
      return false;

   if (!(buffer = (uint8_t*)malloc(vp.width * vp.height * 3)))
      return false;

   if (driver.video && driver.video->read_viewport)
      if (!driver.video->read_viewport(driver.video_data, buffer))
         goto done;

   screenshot_dir = g_settings.screenshot_directory;

   if (!*g_settings.screenshot_directory)
   {
      fill_pathname_basedir(screenshot_path, g_extern.basename,
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
   char screenshot_path[PATH_MAX_LENGTH];
   const void *data           = g_extern.frame_cache.data;
   unsigned width             = g_extern.frame_cache.width;
   unsigned height            = g_extern.frame_cache.height;
   int pitch                  = g_extern.frame_cache.pitch;
   const char *screenshot_dir = g_settings.screenshot_directory;

   if (!*g_settings.screenshot_directory)
   {
      fill_pathname_basedir(screenshot_path, g_extern.basename,
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
   bool viewport_read = false;
   bool ret = true;
   const char *msg = NULL;
   runloop_t *runloop = rarch_main_get_ptr();

   /* No way to infer screenshot directory. */
   if ((!*g_settings.screenshot_directory) && (!*g_extern.basename))
      return false;

   viewport_read = (g_settings.video.gpu_screenshot ||
         ((g_extern.system.hw_render_callback.context_type
         != RETRO_HW_CONTEXT_NONE) && !driver.video->read_frame_raw))
         && driver.video->read_viewport && driver.video->viewport_info;

   if (viewport_read)
   {
#ifdef HAVE_MENU
      /* Avoid taking screenshot of GUI overlays. */
      if (driver.video_poke && driver.video_poke->set_texture_enable)
         driver.video_poke->set_texture_enable(driver.video_data,
               false, false);
#endif

      if (driver.video)
         rarch_render_cached_frame();
   }

   if (viewport_read)
      ret = take_screenshot_viewport();
   else if (g_extern.frame_cache.data &&
         (g_extern.frame_cache.data != RETRO_HW_FRAME_BUFFER_VALID))
      ret = take_screenshot_raw();
   else if (driver.video->read_frame_raw)
   {
      const void* old_data = g_extern.frame_cache.data;
      unsigned old_width   = g_extern.frame_cache.width;
      unsigned old_height  = g_extern.frame_cache.height;
      size_t old_pitch     = g_extern.frame_cache.pitch;

      void* frame_data = driver.video->read_frame_raw
         (driver.video_data, &g_extern.frame_cache.width,
          &g_extern.frame_cache.height, &g_extern.frame_cache.pitch);

      if (frame_data)
      {
         g_extern.frame_cache.data = frame_data;
         ret = take_screenshot_raw();
         free(frame_data);
      }
      else
         ret = false;

      g_extern.frame_cache.data   = old_data;
      g_extern.frame_cache.width  = old_width;
      g_extern.frame_cache.height = old_height;
      g_extern.frame_cache.pitch  = old_pitch;
   }
   else
   {
      RARCH_ERR(RETRO_LOG_TAKE_SCREENSHOT_ERROR);
      ret = false;
   }


   if (ret)
   {
      RARCH_LOG(RETRO_LOG_TAKE_SCREENSHOT);
      msg = RETRO_MSG_TAKE_SCREENSHOT;
   }
   else
   {
      RARCH_WARN(RETRO_LOG_TAKE_SCREENSHOT_FAILED);
      msg = RETRO_MSG_TAKE_SCREENSHOT_FAILED;
   }

   rarch_main_msg_queue_push(msg, 1, runloop->is_paused ? 1 : 180, true);

   if (runloop->is_paused)
      rarch_render_cached_frame();

   return ret;
}


/* Take frame bottom-up. */
bool screenshot_dump(const char *folder, const void *frame,
      unsigned width, unsigned height, int pitch, bool bgr24)
{
   char filename[PATH_MAX_LENGTH];
   char shotname[PATH_MAX_LENGTH];
   struct scaler_ctx scaler = {0};
   FILE *file          = NULL;
   uint8_t *out_buffer = NULL;
   bool ret            = false;

   (void)file;
   (void)out_buffer;
   (void)scaler;

   fill_dated_filename(shotname, IMG_EXT, sizeof(shotname));
   fill_pathname_join(filename, folder, shotname, sizeof(filename));

#ifdef HAVE_ZLIB_DEFLATE
   out_buffer = (uint8_t*)malloc(width * height * 3);
   if (!out_buffer)
      return false;

   scaler.in_width   = width;
   scaler.in_height  = height;
   scaler.out_width  = width;
   scaler.out_height = height;
   scaler.in_stride  = -pitch;
   scaler.out_stride = width * 3;
   scaler.out_fmt = SCALER_FMT_BGR24;
   scaler.scaler_type = SCALER_TYPE_POINT;

   if (bgr24)
      scaler.in_fmt = SCALER_FMT_BGR24;
   else if (g_extern.system.pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888)
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


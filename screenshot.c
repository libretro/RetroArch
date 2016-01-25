/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <file/file_path.h>
#include <compat/strl.h>

#include <formats/rbmp.h>

#if defined(HAVE_ZLIB_DEFLATE) && defined(HAVE_RPNG)
#include <formats/rpng.h>
#define IMG_EXT "png"
#else
#define IMG_EXT "bmp"
#endif

#include "general.h"
#include "msg_hash.h"
#include "gfx/scaler/scaler.h"
#include "retroarch.h"
#include "screenshot.h"
#include "verbosity.h"
#include "gfx/video_driver.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Take frame bottom-up. */
static bool screenshot_dump(const char *folder, const void *frame,
      unsigned width, unsigned height, int pitch, bool bgr24)
{
   bool ret;
   char filename[PATH_MAX_LENGTH];
   char shotname[256];
#if defined(HAVE_ZLIB_DEFLATE) && defined(HAVE_RPNG)
   uint8_t *out_buffer            = NULL;
   struct scaler_ctx scaler       = {0};
#endif

   fill_dated_filename(shotname, IMG_EXT, sizeof(shotname));
   fill_pathname_join(filename, folder, shotname, sizeof(filename));

#ifdef _XBOX1
   d3d_video_t *d3d = (d3d_video_t*)video_driver_get_ptr(true);
   settings_t *settings = config_get_ptr();

   D3DSurface *surf = NULL;
   d3d->dev->GetBackBuffer(-1, D3DBACKBUFFER_TYPE_MONO, &surf);
   ret = XGWriteSurfaceToFile(surf, filename);
   surf->Release();

   if(ret == S_OK)
      ret = true;
   else
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
   free(out_buffer);
#else
   ret = rbmp_save_image(filename, frame, width, height, pitch, bgr24,
        (video_driver_get_pixel_format() == RETRO_PIXEL_FORMAT_XRGB8888) );
#endif
   if (!ret)
      RARCH_ERR("Failed to take screenshot.\n");

   return ret;
}

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

   if (!video_driver_ctl(RARCH_DISPLAY_CTL_READ_VIEWPORT, buffer))
      goto done;

   screenshot_dir = settings->screenshot_directory;

   if (!*settings->screenshot_directory)
   {
      fill_pathname_basedir(screenshot_path, global->name.base,
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
      fill_pathname_basedir(screenshot_path, global->name.base,
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
   bool is_paused;
   bool viewport_read   = false;
   bool ret             = true;
   const char *msg      = NULL;
   settings_t *settings = config_get_ptr();
   global_t *global     = global_get_ptr();

   /* No way to infer screenshot directory. */
   if ((!*settings->screenshot_directory) && (!*global->name.base))
      return false;

   viewport_read = video_driver_ctl(RARCH_DISPLAY_CTL_SUPPORTS_VIEWPORT_READ, NULL);

   if (viewport_read)
   {
      /* Avoid taking screenshot of GUI overlays. */
      video_driver_set_texture_enable(false, false);
      video_driver_ctl(RARCH_DISPLAY_CTL_CACHED_FRAME_RENDER, NULL);
   }

   if (viewport_read)
      ret = take_screenshot_viewport();
   else if (!video_driver_ctl(RARCH_DISPLAY_CTL_CACHED_FRAME_HAS_VALID_FB, NULL))
      ret = take_screenshot_raw();
   else if (video_driver_ctl(RARCH_DISPLAY_CTL_SUPPORTS_READ_FRAME_RAW, NULL))
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
         video_driver_ctl(RARCH_DISPLAY_CTL_CACHED_FRAME_SET_PTR, (void*)frame_data);
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

   is_paused = runloop_ctl(RUNLOOP_CTL_IS_PAUSED, NULL);

   runloop_msg_queue_push(msg, 1, is_paused ? 1 : 180, true);

   if (is_paused)
      video_driver_ctl(RARCH_DISPLAY_CTL_CACHED_FRAME_RENDER, NULL);

   return ret;
}

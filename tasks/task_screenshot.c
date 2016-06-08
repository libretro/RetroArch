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

/* TODO/FIXME - turn this into actual task */

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

#ifdef HAVE_RBMP
#include <formats/rbmp.h>
#endif

#if defined(HAVE_ZLIB_DEFLATE) && defined(HAVE_RPNG)
#include <formats/rpng.h>
#define IMG_EXT "png"
#else
#define IMG_EXT "bmp"
#endif

#include "../general.h"
#include "../msg_hash.h"

#include "../gfx/video_driver.h"
#include "../gfx/video_frame.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

/* Take frame bottom-up. */
static bool screenshot_dump(
      const char *global_name_base,
      const char *folder,
      const void *frame,
      unsigned width,
      unsigned height,
      int pitch, bool bgr24)
{
   char filename[PATH_MAX_LENGTH] = {0};
   char shotname[256]             = {0};
   bool ret                       = false;
   settings_t *settings           = config_get_ptr();
#if defined(HAVE_ZLIB_DEFLATE) && defined(HAVE_RPNG)
   uint8_t *out_buffer            = NULL;
   struct scaler_ctx scaler       = {0};
#endif

   if (settings->auto_screenshot_filename)
   {
      fill_dated_filename(shotname, IMG_EXT, sizeof(shotname));
      fill_pathname_join(filename, folder, shotname, sizeof(filename));
   }
   else
   {
      snprintf(shotname, sizeof(shotname),"%s.png", path_basename(global_name_base));
      fill_pathname_join(filename, folder, shotname, sizeof(filename));
   }

#ifdef _XBOX1
   d3d_video_t *d3d = (d3d_video_t*)video_driver_get_ptr(true);
   D3DSurface *surf = NULL;

   d3d->dev->GetBackBuffer(-1, D3DBACKBUFFER_TYPE_MONO, &surf);
   if (XGWriteSurfaceToFile(surf, filename) == S_OK)
      ret = true;
   surf->Release();
#elif defined(HAVE_ZLIB_DEFLATE) && defined(HAVE_RPNG)
   out_buffer = (uint8_t*)malloc(width * height * 3);
   if (!out_buffer)
      return false;

   video_frame_convert_to_bgr24(
         &scaler,
         out_buffer,
         (const uint8_t*)frame + ((int)height - 1) * pitch,
         width, height,
         -pitch,
         bgr24);

   scaler_ctx_gen_reset(&scaler);

   ret = rpng_save_image_bgr24(
         filename,
         out_buffer,
         width,
         height,
         width * 3
         );
   free(out_buffer);
#elif defined(HAVE_RBMP)
   enum rbmp_source_type bmp_type = RBMP_SOURCE_TYPE_DONT_CARE;

   if (bgr24)
      bmp_type = RBMP_SOURCE_TYPE_BGR24;
   else if (video_driver_get_pixel_format() == RETRO_PIXEL_FORMAT_XRGB8888)
      bmp_type = RBMP_SOURCE_TYPE_XRGB888;

   ret = rbmp_save_image(filename,
         frame,
         width,
         height,
         pitch,
         bmp_type);
#endif

   return ret;
}

static bool take_screenshot_viewport(const char *global_name_base)
{
   char screenshot_path[PATH_MAX_LENGTH] = {0};
   const char *screenshot_dir            = NULL;
   uint8_t *buffer                       = NULL;
   bool retval                           = false;
   struct video_viewport vp              = {0};
   settings_t *settings                  = config_get_ptr();

   video_driver_get_viewport_info(&vp);

   if (!vp.width || !vp.height)
      return false;

   buffer = (uint8_t*)malloc(vp.width * vp.height * 3);
   if (!buffer)
      return false;

   if (!video_driver_read_viewport(buffer))
      goto done;

   screenshot_dir = settings->directory.screenshot;

   if (string_is_empty(screenshot_dir))
   {
      fill_pathname_basedir(screenshot_path, global_name_base,
            sizeof(screenshot_path));
      screenshot_dir = screenshot_path;
   }

   /* Data read from viewport is in bottom-up order, suitable for BMP. */
   if (!screenshot_dump(global_name_base, screenshot_dir, buffer, vp.width, vp.height,
            vp.width * 3, true))
      goto done;

   retval = true;

done:
   if (buffer)
      free(buffer);
   return retval;
}

static bool take_screenshot_raw(const char *global_name_base)
{
   unsigned width, height;
   size_t pitch;
   char screenshot_path[PATH_MAX_LENGTH] = {0};
   const void *data                      = NULL;
   settings_t *settings                  = config_get_ptr();
   const char *screenshot_dir            = settings->directory.screenshot;

   video_driver_cached_frame_get(&data, &width, &height, &pitch);

   if (string_is_empty(settings->directory.screenshot))
   {
      fill_pathname_basedir(screenshot_path, global_name_base,
            sizeof(screenshot_path));
      screenshot_dir = screenshot_path;
   }

   /* Negative pitch is needed as screenshot takes bottom-up,
    * but we use top-down.
    */
   return screenshot_dump(global_name_base, screenshot_dir,
         (const uint8_t*)data + (height - 1) * pitch,
         width, height, -pitch, false);
}

static bool take_screenshot_choice(const char *global_name_base)
{
   settings_t *settings = config_get_ptr();

   /* No way to infer screenshot directory. */
   if (string_is_empty(settings->directory.screenshot) && (!*global_name_base))
      return false;

   if (video_driver_supports_viewport_read())
   {
      /* Avoid taking screenshot of GUI overlays. */
      video_driver_set_texture_enable(false, false);
      video_driver_cached_frame_render();
      return take_screenshot_viewport(global_name_base);
   }

   if (!video_driver_cached_frame_has_valid_framebuffer())
      return take_screenshot_raw(global_name_base);

   if (video_driver_supports_read_frame_raw())
   {
      unsigned old_width, old_height;
      size_t old_pitch;
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
         if (take_screenshot_raw(global_name_base))
            ret = true;
         free(frame_data);
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
   global_t *global           = global_get_ptr();
   char *name_base            = strdup(global->name.base);
   bool            is_paused  = runloop_ctl(RUNLOOP_CTL_IS_PAUSED, NULL);
   bool             ret       = take_screenshot_choice(name_base);
   const char *msg_screenshot = ret 
      ? msg_hash_to_str(MSG_TAKING_SCREENSHOT)  :
        msg_hash_to_str(MSG_FAILED_TO_TAKE_SCREENSHOT);
   const char *msg            = msg_screenshot;

   free(name_base);

   runloop_msg_queue_push(msg, 1, is_paused ? 1 : 180, true);

   if (is_paused)
      video_driver_cached_frame_render();

   return ret;
}

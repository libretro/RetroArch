/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Jean-André Santoni
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

#include "video_texture.h"
#include <file/file_path.h>
#include "video_pixel_converter.h"
#include "video_thread_wrapper.h"

#ifdef HAVE_OPENGL
#include "drivers/gl_common.h"

static void video_texture_png_load_gl(struct texture_image *ti,
      enum texture_filter_type filter_type,
      unsigned *id)
{
   /* Generate the OpenGL texture object */
   glGenTextures(1, (GLuint*)id);
   gl_load_texture_data((GLuint)*id, 
         RARCH_WRAP_EDGE, filter_type,
         4 /* TODO/FIXME - dehardcode */,
         ti->width, ti->height, ti->pixels,
         sizeof(uint32_t) /* TODO/FIXME - dehardcode */
         );
}
#endif

static unsigned video_texture_png_load(void *data,
      enum texture_backend_type type,
      enum texture_filter_type  filter_type)
{
   unsigned id = 0;

   if (!data)
      return 0;

   switch (type)
   {
      case TEXTURE_BACKEND_OPENGL:
#ifdef HAVE_OPENGL
         video_texture_png_load_gl((struct texture_image*)data, filter_type, &id);
#endif
         break;
      case TEXTURE_BACKEND_DEFAULT:
      default:
         break;
   }

   return id;
}

static int video_texture_png_load_wrap(void *data)
{
   return video_texture_png_load(data, TEXTURE_BACKEND_DEFAULT,
         TEXTURE_FILTER_LINEAR);
}

static int video_texture_png_load_wrap_gl_mipmap(void *data)
{
   return video_texture_png_load(data, TEXTURE_BACKEND_OPENGL,
         TEXTURE_FILTER_MIPMAP_LINEAR);
}

static int video_texture_png_load_wrap_gl(void *data)
{
   return video_texture_png_load(data, TEXTURE_BACKEND_OPENGL,
         TEXTURE_FILTER_LINEAR);
}

unsigned video_texture_load(void *data,
      enum texture_backend_type type,
      enum texture_filter_type  filter_type)
{
   settings_t *settings = config_get_ptr();
   const struct retro_hw_render_callback *hw_render =
      (const struct retro_hw_render_callback*)video_driver_callback();

   if (settings->video.threaded && !hw_render->context_type)
   {
      driver_t     *driver = driver_get_ptr();
      thread_video_t *thr  = (thread_video_t*)driver->video_data;
      thread_packet_t pkt = { CMD_CUSTOM_COMMAND };

      if (!thr)
         return 0;

      switch (type)
      {
         case TEXTURE_BACKEND_OPENGL:
            if (filter_type == TEXTURE_FILTER_MIPMAP_LINEAR ||
                  filter_type == TEXTURE_FILTER_MIPMAP_NEAREST)
               pkt.data.custom_command.method = video_texture_png_load_wrap_gl_mipmap;
            else
               pkt.data.custom_command.method = video_texture_png_load_wrap_gl;
            break;
         case TEXTURE_BACKEND_DEFAULT:
         default:
            pkt.data.custom_command.method = video_texture_png_load_wrap;
            break;
      }

      pkt.data.custom_command.data   = (void*)data;

      thr->send_and_wait(thr, &pkt);

      return pkt.data.custom_command.return_value;
   }

   return video_texture_png_load(data, type, filter_type);
}

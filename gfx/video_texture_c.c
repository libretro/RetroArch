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

#include <file/file_path.h>
#include <formats/image.h>

#include "video_driver.h"
#include "video_texture.h"
#include "video_thread_wrapper.h"

#ifdef HAVE_OPENGL

#ifdef __cplusplus
extern "C" {
#endif

void gl_load_texture_data(GLuint id,
      enum gfx_wrap_type wrap_type,
      enum texture_filter_type filter_type,
      unsigned alignment,
      unsigned width, unsigned height,
      const void *frame, unsigned base_size)
{
   GLint mag_filter, min_filter;
   bool want_mipmap = false;
   bool use_rgba    = video_driver_ctl(RARCH_DISPLAY_CTL_SUPPORTS_RGBA, NULL);
   bool rgb32       = (base_size == (sizeof(uint32_t)));
   GLenum wrap      = gl_wrap_type_to_enum(wrap_type);

   glBindTexture(GL_TEXTURE_2D, id);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    
#if defined(HAVE_OPENGLES2) || defined(HAVE_PSGL) || defined(OSX_PPC)
   if (filter_type == TEXTURE_FILTER_MIPMAP_LINEAR)
       filter_type = TEXTURE_FILTER_LINEAR;
   if (filter_type == TEXTURE_FILTER_MIPMAP_NEAREST)
       filter_type = TEXTURE_FILTER_NEAREST;
#endif

   switch (filter_type)
   {
      case TEXTURE_FILTER_MIPMAP_LINEAR:
         min_filter = GL_LINEAR_MIPMAP_NEAREST;
         mag_filter = GL_LINEAR;
         want_mipmap = true;
         break;
      case TEXTURE_FILTER_MIPMAP_NEAREST:
         min_filter = GL_NEAREST_MIPMAP_NEAREST;
         mag_filter = GL_NEAREST;
         want_mipmap = true;
         break;
      case TEXTURE_FILTER_NEAREST:
         min_filter = GL_NEAREST;
         mag_filter = GL_NEAREST;
         break;
      case TEXTURE_FILTER_LINEAR:
      default:
         min_filter = GL_LINEAR;
         mag_filter = GL_LINEAR;
         break;
   }

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);

   glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
   glTexImage2D(GL_TEXTURE_2D,
         0,
         (use_rgba || !rgb32) ? GL_RGBA : RARCH_GL_INTERNAL_FORMAT32,
         width, height, 0,
         (use_rgba || !rgb32) ? GL_RGBA : RARCH_GL_TEXTURE_TYPE32,
         (rgb32) ? RARCH_GL_FORMAT32 : GL_UNSIGNED_SHORT_4_4_4_4, frame);

   if (want_mipmap)
      glGenerateMipmap(GL_TEXTURE_2D);
}

void video_texture_png_load_gl(struct texture_image *ti,
      enum texture_filter_type filter_type,
      uintptr_t *id)
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

#ifdef __cplusplus
}
#endif

#endif

#ifdef HAVE_D3D
#include "common/d3d_common.h"

static void video_texture_png_load_d3d(struct texture_image *ti,
      enum texture_filter_type filter_type,
      uintptr_t *id)
{
   id = (uintptr_t*)d3d_texture_new(NULL, NULL,
         ti->width, ti->height, 1, 
         0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, 0, 0, 0,
         NULL, NULL);
}
#endif

static unsigned video_texture_png_load(void *data,
      enum texture_backend_type type,
      enum texture_filter_type  filter_type)
{
   uintptr_t id = 0;

   if (!data)
      return 0;

   switch (type)
   {
      case TEXTURE_BACKEND_OPENGL:
#ifdef HAVE_OPENGL
         video_texture_png_load_gl((struct texture_image*)data, filter_type, &id);
#endif
         break;
      case TEXTURE_BACKEND_DIRECT3D:
#ifdef HAVE_D3D
         video_texture_png_load_d3d((struct texture_image*)data, filter_type, &id);
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

static int video_texture_png_load_wrap_d3d_mipmap(void *data)
{
   return video_texture_png_load(data, TEXTURE_BACKEND_DIRECT3D,
         TEXTURE_FILTER_MIPMAP_LINEAR);
}

static int video_texture_png_load_wrap_d3d(void *data)
{
   return video_texture_png_load(data, TEXTURE_BACKEND_DIRECT3D,
         TEXTURE_FILTER_LINEAR);
}

#ifdef __cplusplus
extern "C" {
#endif

unsigned video_texture_load(void *data,
      enum texture_backend_type type,
      enum texture_filter_type  filter_type)
{
   settings_t *settings = config_get_ptr();
   const struct retro_hw_render_callback *hw_render =
      (const struct retro_hw_render_callback*)video_driver_callback();

   if (settings->video.threaded && !hw_render->context_type)
   {
      thread_video_t *thr  = (thread_video_t*)video_driver_get_ptr(true);
      thread_packet_t pkt  = { CMD_CUSTOM_COMMAND };

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
         case TEXTURE_BACKEND_DIRECT3D:
            if (filter_type == TEXTURE_FILTER_MIPMAP_LINEAR ||
                  filter_type == TEXTURE_FILTER_MIPMAP_NEAREST)
               pkt.data.custom_command.method = video_texture_png_load_wrap_d3d_mipmap;
            else
               pkt.data.custom_command.method = video_texture_png_load_wrap_d3d;
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

#ifdef __cplusplus
}
#endif

#ifdef HAVE_OPENGL
static void video_texture_gl_unload(uintptr_t *id)
{
   if (id)
      glDeleteTextures(1, (const GLuint*)id);
   *id = 0;
}
#endif

void video_texture_unload(enum texture_backend_type type, uintptr_t *id)
{
   switch (type)
   {
      case TEXTURE_BACKEND_OPENGL:
#ifdef HAVE_OPENGL
         video_texture_gl_unload(id);
#endif
         break;
      case TEXTURE_BACKEND_DIRECT3D:
#ifdef HAVE_D3D
         d3d_texture_free((LPDIRECT3DTEXTURE)id);
#endif
         break;
      case TEXTURE_BACKEND_DEFAULT:
         break;
   }
}

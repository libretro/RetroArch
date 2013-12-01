/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#include "../driver.h"

typedef struct ios_camera
{
   GLuint tex;
} ioscamera_t;

static void *ios_camera_init(const char *device, uint64_t caps, unsigned width, unsigned height)
{
   if ((caps & (1ULL << RETRO_CAMERA_BUFFER_OPENGL_TEXTURE)) == 0)
   {
      RARCH_ERR("ioscamera returns OpenGL texture.\n");
      return NULL;
   }

   ioscamera_t *ioscamera = (ioscamera_t*)calloc(1, sizeof(ioscamera_t));
   if (!ioscamera)
      return NULL;

   return ioscamera;
dealloc:
   free(ioscamera);
   return NULL;
}

static void ios_camera_free(void *data)
{
   ioscamera_t *ioscamera = (ioscamera_t*)data;

   if (ioscamera)
      free(ioscamera);
   ioscamera = NULL;
}

static bool ios_camera_start(void *data)
{
   ioscamera_t *ioscamera = (ioscamera_t*)data;

   glGenTextures(1, &ioscamera->tex);
   glBindTexture(GL_TEXTURE_2D, ioscamera->tex);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

   return true;
}

static void ios_camera_stop(void *data)
{
   ioscamera_t *ioscamera = (ioscamera_t*)data;
   
   if (ioscamera->tex)
      glDeleteTextures(1, &ioscamera->tex);
}

static bool ios_camera_poll(void *data, retro_camera_frame_raw_framebuffer_t frame_raw_cb,
      retro_camera_frame_opengl_texture_t frame_gl_cb)
{
   ioscamera_t *ioscamera = (ioscamera_t*)data;

   bool newFrame = false;
   (void)frame_raw_cb;
   (void)newFrame;

   if (newFrame)
   {
      // FIXME: Identity for now. Use proper texture matrix as returned by iOS Camera (if at all?).
      static const float affine[] = {
         1.0f, 0.0f, 0.0f,
         0.0f, 1.0f, 0.0f,
         0.0f, 0.0f, 1.0f
      };

      if (frame_gl_cb)
        frame_gl_cb(ioscamera->tex,
              GL_TEXTURE_2D,
              affine);
      return true;
   }

   return false;
}

const camera_driver_t camera_ios = {
   ios_camera_init,
   ios_camera_free,
   ios_camera_start,
   ios_camera_stop,
   ios_camera_poll,
   "ios",
};

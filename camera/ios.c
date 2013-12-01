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

// References:
// http://allmybrain.com/2011/12/08/rendering-to-a-texture-with-ios-5-texture-cache-api/
// https://developer.apple.com/library/iOS/samplecode/GLCameraRipple/

#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#include "../driver.h"

typedef struct ios_camera
{
  void *empty;
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

   // TODO - call onCameraInit from RAGameView

   return ioscamera;
dealloc:
   free(ioscamera);
   return NULL;
}

static void ios_camera_free(void *data)
{
   ioscamera_t *ioscamera = (ioscamera_t*)data;
    
   //TODO - call onCameraFree from RAGameView

   if (ioscamera)
      free(ioscamera);
   ioscamera = NULL;
}

static bool ios_camera_start(void *data)
{
   (void)data;

   //TODO - call onCameraStart from RAGAmeView

   return true;
}

static void ios_camera_stop(void *data)
{
   ioscamera_t *ioscamera = (ioscamera_t*)data;
   (void)ioscamera;
    
   //TODO - call onCameraStop from RAGameView
}

static bool ios_camera_poll(void *data, retro_camera_frame_raw_framebuffer_t frame_raw_cb,
      retro_camera_frame_opengl_texture_t frame_gl_cb)
{
   bool newFrame = false;
   (void)data;
   (void)frame_raw_cb;

   // TODO - call onCameraPoll from RAGameView

   if (frame_gl_cb && newFrame)
   {
	   // FIXME: Identity for now. Use proper texture matrix as returned by iOS Camera (if at all?).
	   static const float affine[] = {
		   1.0f, 0.0f, 0.0f,
		   0.0f, 1.0f, 0.0f,
		   0.0f, 0.0f, 1.0f
	   };
       (void)affine;
#if 0
	   frame_gl_cb(CVOpenGLESTextureGetName(ioscamera->renderTexture),
			   GL_TEXTURE_2D,
			   affine);
#endif
   }

   return true;
}

const camera_driver_t camera_ios = {
   ios_camera_init,
   ios_camera_free,
   ios_camera_start,
   ios_camera_stop,
   ios_camera_poll,
   "ios",
};

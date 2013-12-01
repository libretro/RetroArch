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
#include <CoreVideo/CVPixelBuffer.h>
#include <CoreVideo/CVOpenGLESTexture.h>
#include <CoreVideo/CVOpenGLESTextureCache.h>
#include "../driver.h"

typedef struct ios_camera
{
   CFDictionaryRef empty;
   CFMutableDictionaryRef attrs;
   CVPixelBufferRef renderTarget;
   CVOpenGLESTextureRef renderTexture;
   CVOpenGLESTextureCacheRef textureCache;
    GLuint renderFrameBuffer;
} ioscamera_t;

static void *ios_camera_init(const char *device, uint64_t caps, unsigned width, unsigned height)
{
   int ret = 0;
   if ((caps & (1ULL << RETRO_CAMERA_BUFFER_OPENGL_TEXTURE)) == 0)
   {
      RARCH_ERR("ioscamera returns OpenGL texture.\n");
      return NULL;
   }

   ioscamera_t *ioscamera = (ioscamera_t*)calloc(1, sizeof(ioscamera_t));
   if (!ioscamera)
      return NULL;

   ioscamera->empty = CFDictionaryCreate(kCFAllocatorDefault,
      NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
   ioscamera->attrs = CFDictionaryCreateMutable(kCFAllocatorDefault,
      1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

   CFDictionarySetValue(ioscamera->attrs, kCVPixelBufferIOSurfacePropertiesKey,
      ioscamera->empty);

   // TODO: for testing, image is 640x480 for now
   //if (width > 640)
      width = 640;
   //if (height > 480)
      height = 480;

   ret = CVPixelBufferCreate(kCFAllocatorDefault, width, height,
      kCVPixelFormatType_32BGRA, ioscamera->attrs, &ioscamera->renderTarget);
   if (ret != 0)
      goto dealloc;

   // create a texture from our render target.
   // textureCache will be what you previously made with CVOpenGLESTextureCacheCreate
   ret = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
      ioscamera->textureCache, ioscamera->renderTarget, NULL, GL_TEXTURE_2D,
      GL_RGBA, width, height, GL_BGRA, GL_UNSIGNED_BYTE, 0, &ioscamera->renderTexture);
   if (ret != 0)
      goto dealloc;

   return ioscamera;
dealloc:
   free(ioscamera);
   return NULL;
}

static void ios_camera_free(void *data)
{
   ioscamera_t *ioscamera = (ioscamera_t*)data;
    
   //TODO - anything to free here?

   if (ioscamera)
      free(ioscamera);
   ioscamera = NULL;
}

static bool ios_camera_start(void *data)
{
   ioscamera_t *ioscamera = (ioscamera_t*)data;

   glBindTexture(CVOpenGLESTextureGetTarget(ioscamera->renderTexture),
      CVOpenGLESTextureGetName(ioscamera->renderTexture));
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

   // bind the texture to teh fraembuffer you're going to render to
   // (boilerplate code to make a framebuffer not shown)
   glBindFramebuffer(GL_FRAMEBUFFER, ioscamera->renderFrameBuffer);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_TEXTURE_2D, CVOpenGLESTextureGetName(ioscamera->renderTexture), 0);

   return true;
}

static void ios_camera_stop(void *data)
{
   ioscamera_t *ioscamera = (ioscamera_t*)data;
   (void)ioscamera;
    
   //TODO - anything to do here?
}

static bool ios_camera_poll(void *data, retro_camera_frame_raw_framebuffer_t frame_raw_cb,
      retro_camera_frame_opengl_texture_t frame_gl_cb)
{
   ioscamera_t *ioscamera = (ioscamera_t*)data;

   (void)frame_raw_cb;

   // FIXME: Identity for now. Use proper texture matrix as returned by iOS Camera (if at all?).
   static const float affine[] = {
      1.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 1.0f
   };

   if (frame_gl_cb)
     frame_gl_cb(CVOpenGLESTextureGetName(ioscamera->renderTexture),
           GL_TEXTURE_2D,
           affine);
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

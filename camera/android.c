/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
 *  Copyright (C) 2012-2013 - Michael Lelli
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

#include <GLES2/gl2.h>
#include "../driver.h"
#include "../android/native/jni/jni_macros.h"

/* FIXME - we need to seriously profile JNI overhead here - whether we can cache certain
 * objects/variables */
/* FIXME - check whether or not it is safe to attach the thread once at android_init and
 * only detach it when calling android_free - might have to be done per-function but would
 * introduce significant overhead */

typedef struct android_camera
{
   JNIEnv *env;
   JavaVM *java_vm;
   jclass class;     // RetroActivity class
   GLuint tex;
} androidcamera_t;

static void *android_camera_init(const char *device, uint64_t caps, unsigned width, unsigned height)
{
   (void)device;
   (void)width;
   (void)height;

   if ((caps & (1ULL << RETRO_CAMERA_BUFFER_OPENGL_TEXTURE)) == 0)
   {
      RARCH_ERR("androidcamera returns OpenGL texture.\n");
      return NULL;
   }

   struct android_app *android_app = (struct android_app*)g_android;
   androidcamera_t *androidcamera = (androidcamera_t*)calloc(1, sizeof(androidcamera_t));
   if (!androidcamera)
      return NULL;

   androidcamera->java_vm = (JavaVM*)android_app->activity->vm;
   (*androidcamera->java_vm)->AttachCurrentThread(androidcamera->java_vm, &androidcamera->env, 0);

   GET_OBJECT_CLASS(androidcamera->env, androidcamera->class, android_app->activity->clazz);


   jmethodID onCameraInit = NULL;
   GET_METHOD_ID(androidcamera->env, onCameraInit, androidcamera->class, "onCameraInit", "()V");
   if (!onCameraInit)
      return NULL;

   CALL_VOID_METHOD(androidcamera->env, androidcamera->class, onCameraInit);

   return androidcamera;
}

static void android_camera_free(void *data)
{
   struct android_app *android_app = (struct android_app*)g_android;
   androidcamera_t *androidcamera = (androidcamera_t*)data;
   (void)android_app;

   jmethodID onCameraFree = NULL;
   GET_METHOD_ID(androidcamera->env, onCameraFree, androidcamera->class, "onCameraFree", "()V");
   if (!onCameraFree)
      goto end;

   CALL_VOID_METHOD(androidcamera->env, androidcamera->class, onCameraFree);

   end:
   (*androidcamera->java_vm)->DetachCurrentThread(androidcamera->java_vm);

   free(androidcamera);
}

static bool android_camera_start(void *data)
{
   struct android_app *android_app = (struct android_app*)g_android;
   androidcamera_t *androidcamera = (androidcamera_t*)data;

   (void)android_app;
   (void)androidcamera;

   glGenTextures(1, &androidcamera->tex);

   jmethodID onCameraSetTexture = NULL;
   GET_METHOD_ID(androidcamera->env, onCameraSetTexture, androidcamera->class, "onCameraSetTexture", "(I)V");
   if (!onCameraSetTexture)
      goto end;

   CALL_VOID_METHOD_PARAM(androidcamera->env, androidcamera->class, onCameraSetTexture, (int) androidcamera->tex);

   jmethodID onCameraStart = NULL;
   GET_METHOD_ID(androidcamera->env, onCameraSetTexture, androidcamera->class, "onCameraStart", "()V");
   if (!onCameraStart)
      goto end;

   CALL_VOID_METHOD(androidcamera->env, androidcamera->class, onCameraStart);

   return true;

   end:
   return false;
}

static void android_camera_stop(void *data)
{
   struct android_app *android_app = (struct android_app*)g_android;
   androidcamera_t *androidcamera = (androidcamera_t*)data;
   (void)android_app;
   (void)androidcamera;

   jmethodID onCameraStop = NULL;
   GET_METHOD_ID(androidcamera->env, onCameraStop, androidcamera->class, "onCameraStop", "()V");
   if (!onCameraStop)
      return;

   CALL_VOID_METHOD(androidcamera->env, androidcamera->class, onCameraStop);
   
   if (androidcamera->tex)
      glDeleteTextures(1, &androidcamera->tex);
}

static bool android_camera_poll(void *data, retro_camera_frame_raw_framebuffer_t frame_raw_cb,
      retro_camera_frame_opengl_texture_t frame_gl_cb)
{
   struct android_app *android_app = (struct android_app*)g_android;
   androidcamera_t *androidcamera = (androidcamera_t*)data;
   (void)android_app;
   (void)androidcamera;
   (void)frame_raw_cb;

   jmethodID onCameraPoll = NULL;
   GET_METHOD_ID(androidcamera->env, onCameraPoll, androidcamera->class, "onCameraPoll", "()Z");
   if (!onCameraPoll)
      goto end;

   jboolean newFrame;
   CALL_BOOLEAN_METHOD(androidcamera->env, newFrame, androidcamera->class, onCameraPoll);

   if (newFrame)
   {
      // FIXME: Identity for now. Use proper texture matrix as returned by Android Camera.
      static const float affine[] = {
         1.0f, 0.0f, 0.0f,
         0.0f, 1.0f, 0.0f,
         0.0f, 0.0f, 1.0f
      };

      if (frame_gl_cb)
        frame_gl_cb(androidcamera->tex,
              GL_TEXTURE_2D, // TODO: This is likely GL_TEXTURE_EXTERNAL_OES.
              affine);
      return true;
   }

   end:
   return false;
}

const camera_driver_t camera_android = {
   android_camera_init,
   android_camera_free,
   android_camera_start,
   android_camera_stop,
   android_camera_poll,
   "android",
};


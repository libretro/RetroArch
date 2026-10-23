/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "record_android_capture_egl.h"
#if defined(__ANDROID__) || defined(RECORD_ANDROID_CAPTURE_EGL_TEST)
#include <stdlib.h>

struct record_android_capture_egl
{
   record_gpu_mediacodec_config_t gpu;
   dylib_t egl, gles;
   PFNEGLGETPROCADDRESSPROC address;
   PFNEGLTERMINATEPROC terminate;
   bool initialized, failed;
};

static function_t record_ace_resolve(void *context, const char *name)
{
   record_android_capture_egl_t *e = (record_android_capture_egl_t *)context;
   function_t proc = dylib_proc(name[0] == 'e' ? e->egl : e->gles, name);
   if (!proc && e->address)
      proc = (function_t)e->address(name);
   return proc;
}

bool record_android_capture_egl_free(record_android_capture_egl_t *e)
{
   if (!e)
      return true;
   if (e->failed)
      return false;
   if (e->initialized)
   {
      e->initialized = false;
      if (!e->terminate(e->gpu.display))
      {
         e->failed = true;
         return false;
      }
   }
   if (e->gles)
      dylib_close(e->gles);
   if (e->egl)
      dylib_close(e->egl);
   free(e);
   return true;
}

static bool record_ace_choose(
      record_android_capture_egl_t *e, PFNEGLCHOOSECONFIGPROC choose)
{
   static const EGLint tokens[]
         = {EGL_RED_SIZE, EGL_GREEN_SIZE, EGL_BLUE_SIZE, EGL_ALPHA_SIZE};
   EGLint attrs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE,
         EGL_OPENGL_ES3_BIT_KHR, EGL_RECORDABLE_ANDROID, EGL_TRUE, EGL_RED_SIZE,
         8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 0,
         EGL_STENCIL_SIZE, 0, EGL_SAMPLE_BUFFERS, 0, EGL_NONE};
   EGLint count = 0, written = 0, i, j, value, bits = e->gpu.mode ? 10 : 8;
   EGLConfig *configs;
   bool found = false;
   attrs[7] = attrs[9] = attrs[11] = bits;
   if (!choose(e->gpu.display, attrs, NULL, 0, &count) || count <= 0
         || count > 4096)
      return false;
   configs = (EGLConfig *)calloc((size_t)count, sizeof(*configs));
   if (!configs)
      return false;
   if (choose(e->gpu.display, attrs, configs, count, &written) && written > 0
         && written <= count)
      for (i = 0; i < written; i++)
      {
         if (!configs[i])
            continue;
         for (j = 0; j < 4; j++)
         {
            if (!e->gpu.surface.config_attrib(
                      e->gpu.display, configs[i], tokens[j], &value))
               break;
            if (j < 3 ? value != bits
                      : (value != 0 && value != (bits == 10 ? 2 : 8)))
               break;
         }
         if (j == 4)
         {
            e->gpu.config = configs[i];
            found = true;
            break;
         }
      }
   free(configs);
   return found;
}

bool record_android_capture_egl_new(
      unsigned mode, record_android_capture_egl_t **output)
{
   record_android_capture_egl_t *e;
   PFNEGLGETDISPLAYPROC display;
   PFNEGLINITIALIZEPROC initialize;
   PFNEGLCHOOSECONFIGPROC choose;
   if (!output)
      return false;
   *output = NULL;
   if (mode > 2)
      return false;
   e = (record_android_capture_egl_t *)calloc(1, sizeof(*e));
   if (!e)
      return false;
   e->gpu.mode = mode;
   e->egl = dylib_load("libEGL.so");
   e->gles = dylib_load("libGLESv2.so");
   if (!e->egl || !e->gles)
      goto error;
   e->address
         = (PFNEGLGETPROCADDRESSPROC)dylib_proc(e->egl, "eglGetProcAddress");
   e->terminate = (PFNEGLTERMINATEPROC)dylib_proc(e->egl, "eglTerminate");
   display = (PFNEGLGETDISPLAYPROC)dylib_proc(e->egl, "eglGetDisplay");
   initialize = (PFNEGLINITIALIZEPROC)dylib_proc(e->egl, "eglInitialize");
   choose = (PFNEGLCHOOSECONFIGPROC)dylib_proc(e->egl, "eglChooseConfig");
   if (!e->address || !e->terminate || !display || !initialize || !choose
         || !record_android_capture_load_ops(&e->gpu, record_ace_resolve, e))
      goto error;
   e->gpu.display = display(EGL_DEFAULT_DISPLAY);
   if (e->gpu.display == EGL_NO_DISPLAY
         || !initialize(e->gpu.display, NULL, NULL))
      goto error;
   e->initialized = true;
   if (!record_ace_choose(e, choose))
      goto error;
   *output = e;
   return true;
error:
   if (!record_android_capture_egl_free(e))
      *output = e;
   return false;
}

bool record_android_capture_egl_configure(const record_android_capture_egl_t *e,
      record_gpu_mediacodec_config_t *config)
{
   if (!e || !config || !e->initialized || e->failed || !e->gpu.config)
      return false;
   config->surface = e->gpu.surface;
   config->capture = e->gpu.capture;
   config->draw = e->gpu.draw;
   config->display = e->gpu.display;
   config->config = e->gpu.config;
   config->share = EGL_NO_CONTEXT;
   config->mode = e->gpu.mode;
   return true;
}
#else
bool record_android_capture_egl_new(
      unsigned mode, record_android_capture_egl_t **output)
{
   (void)mode;
   if (output)
      *output = NULL;
   return false;
}
bool record_android_capture_egl_configure(
      const record_android_capture_egl_t *egl,
      record_gpu_mediacodec_config_t *config)
{
   (void)egl;
   (void)config;
   return false;
}
bool record_android_capture_egl_free(record_android_capture_egl_t *egl)
{
   return !egl;
}
#endif

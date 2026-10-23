/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <string.h>
#define RECORD_ANDROID_CAPTURE_EGL_TEST
#include "../../../record/record_android_capture_egl.c"
static unsigned libraries, refs, terminated, fault, mode;
static const char *missing;
static bool fallback, bad_configs;
static int egl_library, gles_library, display_token, config_tokens[2];
#define DISPLAY ((EGLDisplay) & display_token)
static void unused(void)
{
}
static EGLDisplay EGLAPIENTRY get_display(EGLNativeDisplayType native)
{
   assert(native == EGL_DEFAULT_DISPLAY);
   return fault == 3 ? EGL_NO_DISPLAY : DISPLAY;
}
static EGLBoolean EGLAPIENTRY initialize(
      EGLDisplay display, EGLint *major, EGLint *minor)
{
   assert(display == DISPLAY && !major && !minor);
   if (fault == 4)
      return EGL_FALSE;
   refs++;
   return EGL_TRUE;
}
static EGLBoolean EGLAPIENTRY terminate(EGLDisplay display)
{
   assert(display == DISPLAY && refs > 7);
   refs--;
   terminated++;
   return fault == 10 ? EGL_FALSE : EGL_TRUE;
}
static EGLBoolean EGLAPIENTRY choose(EGLDisplay display, const EGLint *attrs,
      EGLConfig *configs, EGLint size, EGLint *count)
{
   assert(display == DISPLAY);
   assert(attrs[7] == (mode ? 10 : 8));
   assert(attrs[9] == attrs[7] && attrs[11] == attrs[7]);
   if (!configs)
   {
      assert(!size);
      *count = fault == 5 || bad_configs ? 0 : fault == 6 ? 4097 : 2;
      return fault != 7;
   }
   assert(size == 2);
   configs[0] = (EGLConfig)&config_tokens[0];
   configs[1] = (EGLConfig)&config_tokens[1];
   *count = fault == 8 ? 3 : 2;
   return fault != 9;
}
static EGLBoolean EGLAPIENTRY attrib(
      EGLDisplay display, EGLConfig config, EGLint token, EGLint *value)
{
   assert(display == DISPLAY);
   if (fault == 11)
      return EGL_FALSE;
   *value = token == EGL_ALPHA_SIZE ? (mode ? 2 : 8) : (mode ? 10 : 8);
   if (config == (EGLConfig)&config_tokens[0] || fault == 12)
      *value = 16;
   return EGL_TRUE;
}
static __eglMustCastToProperFunctionPointerType EGLAPIENTRY address(
      const char *name)
{
   if (missing && !strcmp(missing, name))
      return NULL;
   return unused;
}
dylib_t dylib_load(const char *name)
{
   bool egl = !strcmp(name, "libEGL.so");
   assert(egl || !strcmp(name, "libGLESv2.so"));
   if (fault == (egl ? 1u : 2u))
      return NULL;
   libraries++;
   return egl ? &egl_library : &gles_library;
}
function_t dylib_proc(dylib_t library, const char *name)
{
   assert(library == &egl_library || library == &gles_library);
   if (missing && !strcmp(missing, name))
      return NULL;
   if (!strcmp(name, "eglGetProcAddress"))
      return (function_t)address;
   if (!strcmp(name, "eglGetDisplay"))
      return (function_t)get_display;
   if (!strcmp(name, "eglInitialize"))
      return (function_t)initialize;
   if (!strcmp(name, "eglTerminate"))
      return (function_t)terminate;
   if (!strcmp(name, "eglChooseConfig"))
      return (function_t)choose;
   if (!strcmp(name, "eglGetConfigAttrib"))
      return (function_t)attrib;
   if (fallback && !strcmp(name, "glDrawArrays"))
      return NULL;
   return unused;
}
void dylib_close(dylib_t library)
{
   assert(library == &egl_library || library == &gles_library);
   assert(libraries);
   libraries--;
}
int main(void)
{
   static record_gpu_mediacodec_config_t config;
   static const char *names[] = {"eglGetProcAddress", "eglGetDisplay",
         "eglInitialize", "eglTerminate", "eglChooseConfig", "glDrawArrays"};
   record_android_capture_egl_t *owner, *second;
   unsigned i, before;
   refs = 7;
   assert(!record_android_capture_egl_new(3, &owner) && !owner);
   assert(!record_android_capture_egl_new(0, NULL));
   assert(!record_android_capture_egl_configure(NULL, &config));
   assert(record_android_capture_egl_free(NULL));
   for (mode = 0; mode < 3; mode++)
   {
      config.width = 1280;
      config.height = 720;
      config.count = 4;
      config.flip_y = true;
      config.buffers[0] = (struct AHardwareBuffer *)&config;
      fallback = true;
      assert(record_android_capture_egl_new(mode, &owner));
      assert(libraries == 2 && refs == 8);
      assert(!record_android_capture_egl_configure(owner, NULL));
      assert(record_android_capture_egl_configure(owner, &config));
      assert(config.display == DISPLAY && config.mode == mode);
      assert(config.config == (EGLConfig)&config_tokens[1]);
      assert(config.share == EGL_NO_CONTEXT && config.flip_y);
      assert(config.width == 1280 && config.height == 720 && config.count == 4);
      assert(config.buffers[0] == (struct AHardwareBuffer *)&config);
      assert(config.draw.draw == (PFNGLDRAWARRAYSPROC)unused);
      assert(record_android_capture_egl_free(owner));
      assert(!libraries && refs == 7);
   }
   mode = 0;
   for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
   {
      missing = names[i];
      assert(!record_android_capture_egl_new(mode, &owner) && !owner);
      assert(!libraries && refs == 7);
   }
   missing = NULL;
   for (fault = 1; fault <= 12; fault++)
   {
      if (fault == 10)
         continue;
      assert(!record_android_capture_egl_new(mode, &owner) && !owner);
      assert(!libraries && refs == 7);
   }
   fault = 0;
   assert(record_android_capture_egl_new(mode, &owner));
   assert(record_android_capture_egl_new(mode, &second));
   assert(refs == 9 && libraries == 4);
   assert(record_android_capture_egl_free(owner));
   assert(refs == 8 && libraries == 2);
   assert(record_android_capture_egl_configure(second, &config));
   assert(record_android_capture_egl_free(second));
   assert(refs == 7 && !libraries);
   assert(record_android_capture_egl_new(mode, &owner));
   fault = 10;
   before = terminated;
   assert(!record_android_capture_egl_free(owner));
   assert(refs == 7 && libraries == 2 && terminated == before + 1);
   assert(!record_android_capture_egl_configure(owner, &config));
   assert(!record_android_capture_egl_free(owner));
   assert(terminated == before + 1);
   /* Mock display owns no native objects; release retained test storage. */
   dylib_close(owner->egl);
   dylib_close(owner->gles);
   free(owner);
   bad_configs = true;
   before = terminated;
   assert(!record_android_capture_egl_new(mode, &owner) && owner);
   assert(refs == 7 && libraries == 2 && terminated == before + 1);
   assert(!record_android_capture_egl_free(owner));
   assert(terminated == before + 1);
   dylib_close(owner->egl);
   dylib_close(owner->gles);
   free(owner);
   return 0;
}

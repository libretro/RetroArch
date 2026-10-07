/* LD_PRELOAD shim for egl_hw_core_test.py: makes Mesa look like an
 * EGL with less in it, so each way the frontend has of giving the
 * core's context to a thread that does not hold the window surface is
 * taken on a real EGL.  EGL_HW_CORE_SHIM lists what to hide:
 *
 *   surfaceless   EGL_KHR_surfaceless_context is not advertised, so
 *                 the context is bound to a pbuffer
 *   pbuffer       no config says it has pbuffers through
 *                 eglGetConfigAttrib, so the pbuffer's config is one
 *                 searched for (eglChooseConfig still finds them)
 *
 * It also says on stderr when a pbuffer is made, which is how the test
 * knows the pbuffer runs were pbuffer runs.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <EGL/egl.h>

static int shim_hides(const char *what)
{
   const char *shim = getenv("EGL_HW_CORE_SHIM");
   return shim && strstr(shim, what);
}

const char *eglQueryString(EGLDisplay dpy, EGLint name)
{
   static char filtered[16384];
   const char *(*real)(EGLDisplay, EGLint) =
      (const char *(*)(EGLDisplay, EGLint))dlsym(RTLD_NEXT, "eglQueryString");
   const char *s = real(dpy, name);
   char *hit;

   if (     !s || name != EGL_EXTENSIONS || dpy == EGL_NO_DISPLAY
         || !shim_hides("surfaceless") || strlen(s) >= sizeof(filtered))
      return s;
   strcpy(filtered, s);
   while ((hit = strstr(filtered, "EGL_KHR_surfaceless_context")))
      memcpy(hit, "EGL_XXX", 7);
   return filtered;
}

EGLBoolean eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config,
      EGLint attribute, EGLint *value)
{
   EGLBoolean (*real)(EGLDisplay, EGLConfig, EGLint, EGLint*) =
      (EGLBoolean (*)(EGLDisplay, EGLConfig, EGLint, EGLint*))
      dlsym(RTLD_NEXT, "eglGetConfigAttrib");
   EGLBoolean ret = real(dpy, config, attribute, value);

   if (ret && value && attribute == EGL_SURFACE_TYPE && shim_hides("pbuffer"))
      *value &= ~EGL_PBUFFER_BIT;
   return ret;
}

EGLSurface eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config,
      const EGLint *attrib_list)
{
   EGLSurface (*real)(EGLDisplay, EGLConfig, const EGLint*) =
      (EGLSurface (*)(EGLDisplay, EGLConfig, const EGLint*))
      dlsym(RTLD_NEXT, "eglCreatePbufferSurface");
   EGLSurface surf = real(dpy, config, attrib_list);

   fprintf(stderr, "[egl_shim] pbuffer %s\n",
         surf != EGL_NO_SURFACE ? "created" : "refused");
   fflush(stderr);
   return surf;
}

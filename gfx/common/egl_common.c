/*  RetroArch - A frontend for libretro.
 *  copyright (c) 2011-2015 - Daniel De Matteis
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

#include <retro_assert.h>
#include <retro_log.h>

#include "egl_common.h"
#ifdef HAVE_OPENGL
#include "gl_common.h"
#endif

EGLContext g_egl_ctx;
EGLContext g_egl_hw_ctx;
EGLSurface g_egl_surf;
EGLDisplay g_egl_dpy;
EGLConfig g_egl_config;
bool g_use_hw_ctx;

void egl_report_error(void)
{
   EGLint    error = eglGetError();
   const char *str = NULL;
   switch (error)
   {
      case EGL_SUCCESS:
         str = "EGL_SUCCESS";
         break;

      case EGL_BAD_DISPLAY:
         str = "EGL_BAD_DISPLAY";
         break;

      case EGL_BAD_SURFACE:
         str = "EGL_BAD_SURFACE";
         break;

      case EGL_BAD_CONTEXT:
         str = "EGL_BAD_CONTEXT";
         break;

      default:
         str = "Unknown";
         break;
   }

   RARCH_ERR("[EGL]: #0x%x, %s\n", (unsigned)error, str);
}

gfx_ctx_proc_t egl_get_proc_address(const char *symbol)
{
   gfx_ctx_proc_t ret;
   void *sym__ = NULL;

   retro_assert(sizeof(void*) == sizeof(void (*)(void)));

   sym__ = eglGetProcAddress(symbol);
   memcpy(&ret, &sym__, sizeof(void*));

   return ret;
}

void egl_destroy(void)
{
   if (g_egl_dpy)
   {
#ifdef HAVE_OPENGL
      if (g_egl_ctx != EGL_NO_CONTEXT)
      {
         glFlush();
         glFinish();
      }
#endif

      eglMakeCurrent(g_egl_dpy,
            EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      if (g_egl_ctx != EGL_NO_CONTEXT)
         eglDestroyContext(g_egl_dpy, g_egl_ctx);

      if (g_egl_hw_ctx != EGL_NO_CONTEXT)
         eglDestroyContext(g_egl_dpy, g_egl_hw_ctx);

      if (g_egl_surf != EGL_NO_SURFACE)
         eglDestroySurface(g_egl_dpy, g_egl_surf);
      eglTerminate(g_egl_dpy);
   }

   /* Be as careful as possible in deinit.
    * If we screw up, any TTY will not restore.
    */

   g_egl_ctx     = EGL_NO_CONTEXT;
   g_egl_hw_ctx  = EGL_NO_CONTEXT;
   g_egl_surf    = EGL_NO_SURFACE;
   g_egl_dpy     = EGL_NO_DISPLAY;
   g_egl_config  = 0;
}

void egl_bind_hw_render(void *data, bool enable)
{
   g_use_hw_ctx = enable;

   if (!g_egl_dpy || !g_egl_surf)
      return;

   eglMakeCurrent(g_egl_dpy, g_egl_surf,
         g_egl_surf,
         enable ? g_egl_hw_ctx : g_egl_ctx);
}

void egl_swap_buffers(void *data)
{
   eglSwapBuffers(g_egl_dpy, g_egl_surf);
}

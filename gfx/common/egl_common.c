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

#include "../../verbosity.h"

#include "egl_common.h"
#ifdef HAVE_OPENGL
#include "gl_common.h"
#endif

volatile sig_atomic_t g_egl_quit;

EGLContext g_egl_ctx;
EGLContext g_egl_hw_ctx;
EGLSurface g_egl_surf;
EGLDisplay g_egl_dpy;
EGLConfig g_egl_config;
enum gfx_ctx_api g_egl_api;
bool g_egl_inited;
static bool g_egl_use_hw_ctx;
unsigned g_interval;

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

   sym__ = (void*)eglGetProcAddress(symbol);
   memcpy(&ret, &sym__, sizeof(void*));

   return ret;
}

void egl_destroy(void *data)
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
   g_egl_quit    = 0;
   g_egl_api     = GFX_CTX_NONE;
   g_egl_inited  = false;
}

void egl_bind_hw_render(void *data, bool enable)
{
   g_egl_use_hw_ctx = enable;

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

void egl_set_swap_interval(void *data, unsigned interval)
{
   /* Can be called before initialization.
    * Some contexts require that swap interval 
    * is known at startup time.
    */
   g_interval = interval;

   if (!g_egl_dpy)
      return;
   if (!(eglGetCurrentContext()))
      return;

   RARCH_LOG("[EGL]: eglSwapInterval(%u)\n", interval);
   if (!eglSwapInterval(g_egl_dpy, interval))
   {
      RARCH_ERR("[EGL]: eglSwapInterval() failed.\n");
      egl_report_error();
   }
}

void egl_get_video_size(void *data, unsigned *width, unsigned *height)
{
   *width  = 0;
   *height = 0;

   if (g_egl_dpy != EGL_NO_DISPLAY && g_egl_surf != EGL_NO_SURFACE)
   {
      EGLint gl_width, gl_height;

      eglQuerySurface(g_egl_dpy, g_egl_surf, EGL_WIDTH, &gl_width);
      eglQuerySurface(g_egl_dpy, g_egl_surf, EGL_HEIGHT, &gl_height);
      *width  = gl_width;
      *height = gl_height;
   }
}

static void egl_sighandler(int sig)
{
   (void)sig;
   g_egl_quit = 1;
}

void egl_install_sighandlers(void)
{
   struct sigaction sa;

   sa.sa_sigaction = NULL;
   sa.sa_handler   = egl_sighandler;
   sa.sa_flags     = SA_RESTART;
   sigemptyset(&sa.sa_mask);
   sigaction(SIGINT, &sa, NULL);
   sigaction(SIGTERM, &sa, NULL);
}

bool egl_init_context(NativeDisplayType display,
      EGLint *major, EGLint *minor,
     EGLint *n, const EGLint *attrib_ptr)
{
   g_egl_dpy = eglGetDisplay(display);
   if (!g_egl_dpy)
   {
      RARCH_ERR("[EGL]: Couldn't get EGL display.\n");
      return false;
   }

   if (!eglInitialize(g_egl_dpy, major, minor))
      return false;

   RARCH_LOG("[EGL]: EGL version: %d.%d\n", *major, *minor);

   if (!eglChooseConfig(g_egl_dpy, attrib_ptr, &g_egl_config, 1, n) || *n != 1)
      return false;

   return true;
}

bool egl_create_context(EGLint *egl_attribs)
{
   g_egl_ctx = eglCreateContext(g_egl_dpy, g_egl_config, EGL_NO_CONTEXT,
         egl_attribs);

   if (g_egl_ctx == EGL_NO_CONTEXT)
      return false;

   if (g_egl_use_hw_ctx)
   {
      g_egl_hw_ctx = eglCreateContext(g_egl_dpy, g_egl_config, g_egl_ctx,
            egl_attribs);
      RARCH_LOG("[EGL]: Created shared context: %p.\n", (void*)g_egl_hw_ctx);

      if (g_egl_hw_ctx == EGL_NO_CONTEXT)
         return false;;
   }

   return true;
}

bool egl_create_surface(NativeWindowType native_window)
{
   g_egl_surf = eglCreateWindowSurface(g_egl_dpy, g_egl_config, native_window, NULL);
   if (!g_egl_surf || g_egl_surf == EGL_NO_SURFACE)
      return false;

   /* Connect the context to the surface. */
   if (!eglMakeCurrent(g_egl_dpy, g_egl_surf, g_egl_surf, g_egl_ctx))
      return false;

   RARCH_LOG("[EGL]: Current context: %p.\n", (void*)eglGetCurrentContext());

   return true;
}

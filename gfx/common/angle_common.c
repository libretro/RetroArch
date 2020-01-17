/*  RetroArch - A frontend for libretro.
 *  Copyright (c) 2011-2017 - Daniel De Matteis
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

#include <stdlib.h>

#include <retro_assert.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_OPENGL
#include "gl_common.h"
#endif

#include "egl_common.h"
#include "angle_common.h"
#include "../../verbosity.h"
#include "../../frontend/frontend_driver.h"

 /* Normal DirectX 11 backend */
const EGLint backendD3D11[] =
{
    EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
    EGL_NONE,
};

/* DirectX 11 Feature Level 9_3 backend, for Windows Mobile (UWP) */
const EGLint backendD3D11_FL9_3[] =
{
    EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
    EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE, 9,
    EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE, 3,
    EGL_NONE,
};

/* Normal DirectX 9 backend */
const EGLint backendD3D9[] =
{
    EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE,
    EGL_NONE,
};

/* WARP software renderer */
const EGLint backendWARP[] =
{
    EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
    EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_WARP_ANGLE,
    EGL_NONE,
};

const EGLint* backendList[] = {
   backendD3D11,
#ifdef __WINRT__
   backendD3D11_FL9_3,
#else
   backendD3D9,
#endif
   backendWARP,
   NULL
};

const char* backendNamesList[] = {
   "D3D11 Backend",
#ifdef __WINRT__
   "D3D11 FL9_3 Backend",
#else 
   "D3D9 Backend",
#endif
   "D3D11 WARP Software Render Backend",
   NULL
};

/* Try initializing EGL with the backend specified in display_attr. */
static bool angle_try_initialize(egl_ctx_data_t* egl,
   void* display_data, const EGLint* display_attr,
   EGLint* major, EGLint* minor)
{
   EGLDisplay dpy    = EGL_NO_DISPLAY;
#if defined(HAVE_DYNAMIC) && defined(HAVE_DYNAMIC_EGL)
   if (!egl_init_dll())
      return false;
#endif

   PFNEGLGETPLATFORMDISPLAYEXTPROC ptr_eglGetPlatformDisplayEXT =
      (PFNEGLGETPLATFORMDISPLAYEXTPROC)egl_get_proc_address("eglGetPlatformDisplayEXT");

   if (!ptr_eglGetPlatformDisplayEXT)
      return false;

   dpy = ptr_eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, display_data, display_attr);
   if (dpy == EGL_NO_DISPLAY)
      return false;

   if (!egl_initialize(dpy, major, minor))
   {
      egl_terminate(egl->dpy);
      return false;
   }

   egl->dpy = dpy;
   return true;
}

/* Special version of egl_init_context that tries
 * all supported render backend of ANGLE. */
bool angle_init_context(egl_ctx_data_t *egl,
      void *display_data,
      EGLint *major, EGLint *minor,
      EGLint *count, const EGLint *attrib_ptr,
      egl_accept_config_cb_t cb)
{
   int j;
   bool success       = false;

   for (j = 0; backendNamesList[j] != NULL; j++)
   {
      RARCH_LOG("[ANGLE] Trying %s...\n", backendNamesList[j]);
      if (angle_try_initialize(egl, display_data, backendList[j], major, minor))
      {
         success = true;
         break;
      }
   }

   if (!success)
      return false;

   RARCH_LOG("[EGL]: EGL version: %d.%d\n", *major, *minor);

   return egl_init_context_common(egl, count, attrib_ptr, cb, display_data);
}

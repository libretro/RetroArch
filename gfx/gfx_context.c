/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "../general.h"
#include "gfx_context.h"
#include "../general.h"
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

static const gfx_ctx_driver_t *gfx_ctx_drivers[] = {
#if defined(__CELLOS_LV2__)
   &gfx_ctx_ps3,
#endif
#if defined(HAVE_WIN32_D3D9) || defined(_XBOX)
   &gfx_ctx_d3d,
#endif
#if defined(HAVE_VIDEOCORE)
   &gfx_ctx_videocore,
#endif
#if defined(HAVE_MALI_FBDEV)
   &gfx_ctx_mali_fbdev,
#endif
#if defined(HAVE_VIVANTE_FBDEV)
   &gfx_ctx_vivante_fbdev,
#endif
#if defined(_WIN32) && defined(HAVE_OPENGL)
   &gfx_ctx_wgl,
#endif
#if defined(HAVE_X11) && defined(HAVE_OPENGL) && !defined(HAVE_OPENGLES)
   &gfx_ctx_glx,
#endif
#if defined(HAVE_WAYLAND) && defined(HAVE_OPENGL) && defined(HAVE_EGL)
   &gfx_ctx_wayland,
#endif
#if defined(HAVE_X11) && defined(HAVE_OPENGL) && defined(HAVE_EGL)
   &gfx_ctx_x_egl,
#endif
#if defined(HAVE_KMS)
   &gfx_ctx_drm_egl,
#endif
#if defined(ANDROID)
   &gfx_ctx_android,
#endif
#if defined(__QNX__)
   &gfx_ctx_bbqnx,
#endif
#if defined(IOS) || defined(OSX)
   /* < Don't use __APPLE__ as it breaks basic SDL builds */
   &gfx_ctx_apple,
#endif
#if (defined(HAVE_SDL) || defined(HAVE_SDL2)) && defined(HAVE_OPENGL)
   &gfx_ctx_sdl_gl,
#endif
#ifdef EMSCRIPTEN
   &gfx_ctx_emscripten,
#endif
   &gfx_ctx_null,
   NULL
};

static int find_gfx_ctx_driver_index(const char *drv)
{
   unsigned i;
   for (i = 0; gfx_ctx_drivers[i]; i++)
      if (strcasecmp(drv, gfx_ctx_drivers[i]->ident) == 0)
         return i;
   return -1;
}

void find_prev_gfx_context_driver(void)
{
   int i = find_gfx_ctx_driver_index(g_settings.video.context_driver);
   if (i > 0)
      strlcpy(g_settings.video.context_driver, gfx_ctx_drivers[i - 1]->ident,
            sizeof(g_settings.video.context_driver));
   else
      RARCH_WARN("Couldn't find any previous video context driver.\n");
}

void find_next_context_driver(void)
{
   int i = find_gfx_ctx_driver_index(g_settings.video.context_driver);
   if (i >= 0 && gfx_ctx_drivers[i + 1])
      strlcpy(g_settings.video.context_driver, gfx_ctx_drivers[i + 1]->ident,
            sizeof(g_settings.video.context_driver));
   else
      RARCH_WARN("Couldn't find any next video context driver.\n");
}

static const gfx_ctx_driver_t *ctx_init(void *data,
      const gfx_ctx_driver_t *ctx,
      const char *drv,
      enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx)
{
   if (ctx->bind_api(data, api, major, minor))
   {
      bool initialized = ctx->init(data);

      if (!initialized)
         return NULL;

      if (ctx->bind_hw_render)
         ctx->bind_hw_render(data,
               g_settings.video.shared_context && hw_render_ctx);

      return ctx;
   }

   return NULL;
}

static const gfx_ctx_driver_t *gfx_ctx_find_driver(void *data,
      const char *ident,
      enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx)
{
   unsigned d;
   const gfx_ctx_driver_t *ctx = NULL;
   int i = find_gfx_ctx_driver_index(ident);
   if (i >= 0)
#if 0
   {
      ctx = ctx_init(data, gfx_ctx_drivers[i], ident,
            api, major, minor, hw_render_ctx);
      if (ctx)
         return ctx;
   }
#else
   return ctx_init(data, gfx_ctx_drivers[i], ident,
         api, major, minor, hw_render_ctx);
#endif

   RARCH_ERR("Couldn't find or initialize any video context driver named \"%s\"\n", ident);
   RARCH_LOG_OUTPUT("Available video context drivers are:\n");
   for (d = 0; gfx_ctx_drivers[d]; d++)
      RARCH_LOG_OUTPUT("\t%s\n", gfx_ctx_drivers[d]->ident);

   RARCH_WARN("Going to default to first suitable video context driver ...\n");

   for (i = 0; gfx_ctx_drivers[i]; i++)
   {
      ctx = ctx_init(data, gfx_ctx_drivers[i], ident,
            api, major, minor, hw_render_ctx);
      if (ctx)
      {
         RARCH_LOG("Selected %s driver.\n", gfx_ctx_drivers[i]->ident);
#if 0
         strlcpy(g_settings.video.context_driver, gfx_ctx_drivers[i]->ident,
               sizeof(g_settings.video.context_driver));
#endif
         return ctx;
      }
   }

   return NULL;
}


const gfx_ctx_driver_t *gfx_ctx_init_first(void *data,
      const char *drv,
      enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx)
{
   const gfx_ctx_driver_t *ctx = (const gfx_ctx_driver_t*)
      gfx_ctx_find_driver(data, drv, api, major, minor,
            hw_render_ctx);

   if (!ctx)
      return NULL;

   return ctx;
}

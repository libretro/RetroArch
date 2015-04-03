/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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
#include "video_context_driver.h"
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

static const gfx_ctx_driver_t *gfx_ctx_drivers[] = {
#if defined(__CELLOS_LV2__)
   &gfx_ctx_ps3,
#endif
#if defined(HAVE_D3D)
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
#if defined(HAVE_X11) && defined(HAVE_OPENGL)
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
   /* Don't use __APPLE__ as it breaks basic SDL builds. */
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

/**
 * find_gfx_ctx_driver_index:
 * @ident                      : Identifier of resampler driver to find.
 *
 * Finds graphics context driver index by @ident name.
 *
 * Returns: graphics context driver index if driver was found, otherwise
 * -1.
 **/
static int find_gfx_ctx_driver_index(const char *ident)
{
   unsigned i;
   for (i = 0; gfx_ctx_drivers[i]; i++)
      if (strcasecmp(ident, gfx_ctx_drivers[i]->ident) == 0)
         return i;
   return -1;
}

/**
 * find_prev_context_driver:
 *
 * Finds previous driver in graphics context driver array.
 **/
void find_prev_gfx_context_driver(void)
{
   settings_t *settings = config_get_ptr();
   int i = find_gfx_ctx_driver_index(settings->video.context_driver);
   
   if (i > 0)
   {
      strlcpy(settings->video.context_driver, gfx_ctx_drivers[i - 1]->ident,
            sizeof(settings->video.context_driver));
   }
   else
      RARCH_WARN("Couldn't find any previous video context driver.\n");
}

/**
 * find_next_context_driver:
 *
 * Finds next driver in graphics context driver array.
 **/
void find_next_context_driver(void)
{
   settings_t *settings = config_get_ptr();
   int i = find_gfx_ctx_driver_index(settings->video.context_driver);

   if (i >= 0 && gfx_ctx_drivers[i + 1])
   {
      strlcpy(settings->video.context_driver, gfx_ctx_drivers[i + 1]->ident,
            sizeof(settings->video.context_driver));
   }
   else
      RARCH_WARN("Couldn't find any next video context driver.\n");
}

/**
 * gfx_ctx_init:
 * @data                    : Input data.
 * @ctx                     : Graphics context driver to initialize.
 * @ident                   : Identifier of graphics context driver to find.
 * @api                     : API of higher-level graphics API.
 * @major                   : Major version number of higher-level graphics API.
 * @minor                   : Minor version number of higher-level graphics API.
 * @hw_render_ctx           : Request a graphics context driver capable of
 *                            hardware rendering?
 *
 * Initialize graphics context driver.
 *
 * Returns: graphics context driver if successfully initialized, otherwise NULL.
 **/
static const gfx_ctx_driver_t *gfx_ctx_init(void *data,
      const gfx_ctx_driver_t *ctx,
      const char *ident,
      enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx)
{
   settings_t *settings = config_get_ptr();

   if (ctx->bind_api(data, api, major, minor))
   {
      bool initialized = ctx->init(data);

      if (!initialized)
         return NULL;

      if (ctx->bind_hw_render)
         ctx->bind_hw_render(data,
               settings->video.shared_context && hw_render_ctx);

      return ctx;
   }

#ifndef _WIN32
   RARCH_WARN("Failed to bind API (#%u, version %u.%u) on context driver \"%s\".\n",
         (unsigned)api, major, minor, ctx->ident);
#endif

   return NULL;
}

/**
 * gfx_ctx_find_driver:
 * @data                    : Input data.
 * @ident                   : Identifier of graphics context driver to find.
 * @api                     : API of higher-level graphics API.
 * @major                   : Major version number of higher-level graphics API.
 * @minor                   : Minor version number of higher-level graphics API.
 * @hw_render_ctx           : Request a graphics context driver capable of
 *                            hardware rendering?
 *
 * Finds graphics context driver and initializes.
 *
 * Returns: graphics context driver if found, otherwise NULL.
 **/
static const gfx_ctx_driver_t *gfx_ctx_find_driver(void *data,
      const char *ident,
      enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx)
{
   const gfx_ctx_driver_t *ctx = NULL;
   int i = find_gfx_ctx_driver_index(ident);

   if (i >= 0)
      return gfx_ctx_init(data, gfx_ctx_drivers[i], ident,
            api, major, minor, hw_render_ctx);

   for (i = 0; gfx_ctx_drivers[i]; i++)
   {
      ctx = gfx_ctx_init(data, gfx_ctx_drivers[i], ident,
            api, major, minor, hw_render_ctx);
      if (ctx)
         return ctx;
   }

   return NULL;
}

/**
 * gfx_ctx_init_first:
 * @data                    : Input data.
 * @ident                   : Identifier of graphics context driver to find.
 * @api                     : API of higher-level graphics API.
 * @major                   : Major version number of higher-level graphics API.
 * @minor                   : Minor version number of higher-level graphics API.
 * @hw_render_ctx           : Request a graphics context driver capable of
 *                            hardware rendering?
 *
 * Finds first suitable graphics context driver and initializes.
 *
 * Returns: graphics context driver if found, otherwise NULL.
 **/
const gfx_ctx_driver_t *gfx_ctx_init_first(void *data,
      const char *ident, enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx)
{
   return gfx_ctx_find_driver(data, ident, api, major, minor, hw_render_ctx);
}

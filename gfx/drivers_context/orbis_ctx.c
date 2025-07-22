/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2018      - M4xw
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
#include <string/stdstring.h>
#include <compat/strl.h>
#include <piglet.h>
#include <orbis/libkernel.h>
#include <EGL/egl.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_EGL
#include "../common/egl_common.h"
#endif

#include "../../frontend/frontend_driver.h"
#include "../../configuration.h"
#include <defines/ps4_defines.h>

#ifdef HAVE_EGL
#include <piglet.h>
#include "../common/egl_common.h"
#endif

#define ATTR_ORBISGL_WIDTH 1920
#define ATTR_ORBISGL_HEIGHT 1080

#if defined(HAVE_OOSDK)
#define SIZEOF_SCE_SHDR_CACHE_CONFIG 0x10C
TYPE_BEGIN(struct _SceShdrCacheConfig, SIZEOF_SCE_SHDR_CACHE_CONFIG);
	TYPE_FIELD(uint32_t ver, 0x00);
	TYPE_FIELD(uint32_t unk1, 0x04);
	TYPE_FIELD(uint32_t unk2, 0x08);
	TYPE_FIELD(char cache_dir[128], 0x0C);
TYPE_END();
typedef struct _SceShdrCacheConfig SceShdrCacheConfig;

bool scePigletSetShaderCacheConfiguration(const SceShdrCacheConfig *config);
#endif

typedef struct
{
#ifdef HAVE_EGL
    egl_ctx_data_t egl;
    ScePglConfig pgl_config;
#if defined(HAVE_OOSDK)
    SceShdrCacheConfig shdr_cache_config;
#endif
#endif
    SceWindow native_window;
    bool resize;
    unsigned width, height;
    float refresh_rate;
} orbis_ctx_data_t;

/* TODO/FIXME - static globals */
static enum gfx_ctx_api ctx_orbis_api = GFX_CTX_OPENGL_API;

/* TODO/FIXME - global reference */
extern bool platform_orbis_has_focus;
extern SceKernelModule s_piglet_module;

void orbis_ctx_destroy(void *data)
{
   orbis_ctx_data_t *ctx_orbis = (orbis_ctx_data_t *)data;

   if (ctx_orbis)
   {
#ifdef HAVE_EGL
      egl_destroy(&ctx_orbis->egl);
#endif
      ctx_orbis->resize = false;
      free(ctx_orbis);
   }
}

static void orbis_ctx_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   orbis_ctx_data_t
      *ctx_orbis = (orbis_ctx_data_t *)data;

   *width        = ATTR_ORBISGL_WIDTH;
   *height       = ATTR_ORBISGL_HEIGHT;
}

static void *orbis_ctx_init(void *video_driver)
{
#if defined(HAVE_OOSDK)
   const char *shdr_cache_dir;
#endif
#ifdef HAVE_EGL
   int ret;
   EGLint n;
   EGLint major, minor;
   static const EGLint attribs[] = {
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_DEPTH_SIZE, 16,
      EGL_STENCIL_SIZE, 0,
      EGL_SAMPLE_BUFFERS, 0,
      EGL_SAMPLES, 0,
#if defined(HAVE_OPENGLES3)
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
#else
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
#endif
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_NONE};
#endif
   orbis_ctx_data_t *ctx_orbis   = (orbis_ctx_data_t *)
      calloc(1, sizeof(*ctx_orbis));

   if (!ctx_orbis)
      return NULL;

#ifdef HAVE_EGL

   memset(&ctx_orbis->pgl_config, 0, sizeof(ctx_orbis->pgl_config));

   ctx_orbis->pgl_config.size                    = sizeof(ctx_orbis->pgl_config);
   ctx_orbis->pgl_config.flags                   =
        SCE_PGL_FLAGS_USE_COMPOSITE_EXT
      | SCE_PGL_FLAGS_USE_FLEXIBLE_MEMORY
      | 0x60;
   ctx_orbis->pgl_config.processOrder            = 1;
   ctx_orbis->pgl_config.systemSharedMemorySize  = 0x1000000;
   ctx_orbis->pgl_config.videoSharedMemorySize   = 0x3000000;
   ctx_orbis->pgl_config.maxMappedFlexibleMemory = 0xFFFFFFFF;
   ctx_orbis->pgl_config.drawCommandBufferSize   = 0x100000;
   ctx_orbis->pgl_config.lcueResourceBufferSize  = 0x1000000;
   ctx_orbis->pgl_config.dbgPosCmd_0x40          = ATTR_ORBISGL_WIDTH;
   ctx_orbis->pgl_config.dbgPosCmd_0x44          = ATTR_ORBISGL_HEIGHT;
   ctx_orbis->pgl_config.dbgPosCmd_0x48          = 0;
   ctx_orbis->pgl_config.dbgPosCmd_0x4C          = 0;
   ctx_orbis->pgl_config.unk_0x5C                = 2;

   if (!(ret = scePigletSetConfigurationVSH(&ctx_orbis->pgl_config)))
      goto error;

#if defined(HAVE_OOSDK)
   memset(&ctx_orbis->shdr_cache_config, 0, sizeof(ctx_orbis->shdr_cache_config));
   shdr_cache_dir                   = "/data/retroarch/temp/";
   ctx_orbis->shdr_cache_config.ver = 0x00010064;
   snprintf(ctx_orbis->shdr_cache_config.cache_dir,
         strlen(shdr_cache_dir) + 1, "%s",
         shdr_cache_dir);

   if (!(ret =
            scePigletSetShaderCacheConfiguration(&ctx_orbis->shdr_cache_config)))
      goto error;
#endif

   if (!egl_init_context(&ctx_orbis->egl, EGL_NONE, EGL_DEFAULT_DISPLAY,
            &major, &minor, &n, attribs, NULL))
   {
      egl_report_error();
      goto error;
   }
#endif

   return ctx_orbis;

error:
   orbis_ctx_destroy(video_driver);
   return NULL;
}

static void orbis_ctx_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height)
{
    unsigned new_width, new_height;

    orbis_ctx_get_video_size(data, &new_width, &new_height);

    if (new_width != *width || new_height != *height)
    {
        *width  = new_width;
        *height = new_height;
        *resize = true;
    }

    *quit       = (bool)false;
}

static bool orbis_ctx_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
    /* Create an EGL rendering context */
    static const EGLint
       contextAttributeList[]       =
        {
#if defined(HAVE_OPENGLES3)
            EGL_CONTEXT_CLIENT_VERSION, 3, // GLES3
#else
            EGL_CONTEXT_CLIENT_VERSION, 2, // GLES2
#endif
            EGL_NONE};

    orbis_ctx_data_t *ctx_orbis     = (orbis_ctx_data_t *)data;

    ctx_orbis->width                = ATTR_ORBISGL_WIDTH;
    ctx_orbis->height               = ATTR_ORBISGL_HEIGHT;

    ctx_orbis->native_window.width  = ctx_orbis->width;
    ctx_orbis->native_window.height = ctx_orbis->height;

    ctx_orbis->refresh_rate = 60;

#ifdef HAVE_EGL
    if (!egl_create_context(&ctx_orbis->egl, contextAttributeList))
        goto error;
    if (!egl_create_surface(&ctx_orbis->egl, &ctx_orbis->native_window))
        goto error;
#endif

    return true;

error:
#ifdef HAVE_EGL
    egl_report_error();
#endif
    orbis_ctx_destroy(data);

    return false;
}

static void orbis_ctx_input_driver(void *data,
      const char *name,
      input_driver_t **input, void **input_data)
{
    *input      = NULL;
    *input_data = NULL;
}

static enum gfx_ctx_api orbis_ctx_get_api(void *data) { return ctx_orbis_api; }

static bool orbis_ctx_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
    ctx_orbis_api = api;

#ifdef HAVE_EGL
    if (api == GFX_CTX_OPENGL_ES_API)
        if (egl_bind_api(EGL_OPENGL_ES_API))
            return true;
#endif

    return false;
}

static bool orbis_ctx_has_focus(void *data) { return true; }

static bool orbis_ctx_suppress_screensaver(void *data, bool enable) { return false; }

static void orbis_ctx_set_swap_interval(void *data,
      int swap_interval)
{
#ifdef HAVE_EGL
   orbis_ctx_data_t *ctx_orbis = (orbis_ctx_data_t *)data;
   egl_set_swap_interval(&ctx_orbis->egl, 0);
#endif
}

static void orbis_ctx_swap_buffers(void *data)
{
#ifdef HAVE_EGL
   orbis_ctx_data_t *ctx_orbis = (orbis_ctx_data_t *)data;
   egl_swap_buffers(&ctx_orbis->egl);
#endif
}

static gfx_ctx_proc_t orbis_ctx_get_proc_address(const char *symbol)
{
   gfx_ctx_proc_t ptr_sym = NULL;
#ifdef HAVE_EGL
   ptr_sym = egl_get_proc_address(symbol);
#endif
   if (!ptr_sym && s_piglet_module > 0)
      sceKernelDlsym(s_piglet_module, symbol, (void **)&ptr_sym);
   return ptr_sym;
}

static void orbis_ctx_bind_hw_render(void *data, bool enable)
{
#ifdef HAVE_EGL
   orbis_ctx_data_t *ctx_orbis = (orbis_ctx_data_t *)data;
   egl_bind_hw_render(&ctx_orbis->egl, enable);
#endif
}

static uint32_t orbis_ctx_get_flags(void *data)
{
   uint32_t flags = 0;

   if (string_is_equal(video_driver_get_ident(), "glcore"))
   {
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
      BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif
   }
   else
   {
      BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);
   }

   return flags;
}

static void orbis_ctx_set_flags(void *data, uint32_t flags) { }

static float orbis_ctx_get_refresh_rate(void *data)
{
   orbis_ctx_data_t *ctx_orbis = (orbis_ctx_data_t *)data;
   return ctx_orbis->refresh_rate;
}

const gfx_ctx_driver_t orbis_ctx = {
    orbis_ctx_init,
    orbis_ctx_destroy,
    orbis_ctx_get_api,
    orbis_ctx_bind_api,
    orbis_ctx_set_swap_interval,
    orbis_ctx_set_video_mode,
    orbis_ctx_get_video_size,
    orbis_ctx_get_refresh_rate,
    NULL, /* get_video_output_size */
    NULL, /* get_video_output_prev */
    NULL, /* get_video_output_next */
    NULL, /* get_metrics */
    NULL,
    NULL, /* update_title */
    orbis_ctx_check_window,
    NULL, /* set_resize */
    orbis_ctx_has_focus,
    orbis_ctx_suppress_screensaver,
    false, /* has_windowed */
    orbis_ctx_swap_buffers,
    orbis_ctx_input_driver,
#ifdef HAVE_EGL
    egl_get_proc_address,
#else
    NULL,
#endif
    NULL,
    NULL,
    NULL,
    "egl_orbis",
    orbis_ctx_get_flags,
    orbis_ctx_set_flags,
    orbis_ctx_bind_hw_render,
    NULL,
    NULL};

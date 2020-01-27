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

#include <compat/strl.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../common/orbis_common.h"
#include "../../frontend/frontend_driver.h"
#include "../../configuration.h"

static enum gfx_ctx_api ctx_orbis_api = GFX_CTX_OPENGL_API;

orbis_ctx_data_t *nx_ctx_ptr = NULL;

extern bool platform_orbis_has_focus;

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
    orbis_ctx_data_t *ctx_orbis = (orbis_ctx_data_t *)data;

    *width = ATTR_ORBISGL_WIDTH;
    *height = ATTR_ORBISGL_HEIGHT;
}

static void *orbis_ctx_init(video_frame_info_t *video_info, void *video_driver)
{
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
         EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
         EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
         EGL_NONE};
#endif

    orbis_ctx_data_t *ctx_orbis = (orbis_ctx_data_t *)calloc(1, sizeof(*ctx_orbis));

    if (!ctx_orbis)
        return NULL;

    nx_ctx_ptr = ctx_orbis;

#ifdef HAVE_EGL

   memset(&ctx_orbis->pgl_config, 0, sizeof(ctx_orbis->pgl_config));
   {
      ctx_orbis->pgl_config.size=sizeof(ctx_orbis->pgl_config);
      ctx_orbis->pgl_config.flags=SCE_PGL_FLAGS_USE_COMPOSITE_EXT | SCE_PGL_FLAGS_USE_FLEXIBLE_MEMORY | 0x60;
      ctx_orbis->pgl_config.processOrder=1;
      ctx_orbis->pgl_config.systemSharedMemorySize=0x200000;
      ctx_orbis->pgl_config.videoSharedMemorySize=0x2400000;
      ctx_orbis->pgl_config.maxMappedFlexibleMemory=0xAA00000;
      ctx_orbis->pgl_config.drawCommandBufferSize=0xC0000;
      ctx_orbis->pgl_config.lcueResourceBufferSize=0x10000;
      ctx_orbis->pgl_config.dbgPosCmd_0x40=ATTR_ORBISGL_WIDTH;
      ctx_orbis->pgl_config.dbgPosCmd_0x44=ATTR_ORBISGL_HEIGHT;
      ctx_orbis->pgl_config.dbgPosCmd_0x48=0;
      ctx_orbis->pgl_config.dbgPosCmd_0x4C=0;
      ctx_orbis->pgl_config.unk_0x5C=2;
   }
    ret = scePigletSetConfigurationVSH(&ctx_orbis->pgl_config);
    if (!ret)
    {
		  printf("[ORBISGL] scePigletSetConfigurationVSH failed 0x%08X.\n",ret);
        goto error;
    }

    if (!egl_init_context(&ctx_orbis->egl, EGL_NONE, EGL_DEFAULT_DISPLAY,
                          &major, &minor, &n, attribs, NULL))
    {
        egl_report_error();
        printf("[ORBIS]: EGL error: %d.\n", eglGetError());
        goto error;
    }
#endif

    return ctx_orbis;

error:
    orbis_ctx_destroy(video_driver);
    return NULL;
}

static void orbis_ctx_check_window(void *data, bool *quit,
                                    bool *resize, unsigned *width, unsigned *height, bool is_shutdown)
{
    unsigned new_width, new_height;

    orbis_ctx_get_video_size(data, &new_width, &new_height);

    if (new_width != *width || new_height != *height)
    {
        *width = new_width;
        *height = new_height;
        *resize = true;
    }

    *quit = (bool)false;
}

static bool orbis_ctx_set_video_mode(void *data,
                                      video_frame_info_t *video_info,
                                      unsigned width, unsigned height,
                                      bool fullscreen)
{
    /* Create an EGL rendering context */
    static const EGLint contextAttributeList[] =
        {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE};

    orbis_ctx_data_t *ctx_orbis = (orbis_ctx_data_t *)data;

    ctx_orbis->width = ATTR_ORBISGL_WIDTH;
    ctx_orbis->height = ATTR_ORBISGL_HEIGHT;

    ctx_orbis->native_window.width = ctx_orbis->width;
    ctx_orbis->native_window.height = ctx_orbis->height;

    ctx_orbis->refresh_rate = 60;

#ifdef HAVE_EGL
    if (!egl_create_context(&ctx_orbis->egl, contextAttributeList))
        goto error;
#endif

#ifdef HAVE_EGL
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

static enum gfx_ctx_api orbis_ctx_get_api(void *data)
{
    return ctx_orbis_api;
}

static bool orbis_ctx_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
    (void)data;
    ctx_orbis_api = api;

#ifdef HAVE_EGL
    if (api == GFX_CTX_OPENGL_ES_API)
        if (egl_bind_api(EGL_OPENGL_ES_API))
            return true;
#endif

    return false;
}

static bool orbis_ctx_has_focus(void *data)
{
    (void)data;
    return true;
}

static bool orbis_ctx_suppress_screensaver(void *data, bool enable)
{
    (void)data;
    (void)enable;
    return false;
}

static void orbis_ctx_set_swap_interval(void *data,
                                         int swap_interval)
{
    orbis_ctx_data_t *ctx_orbis = (orbis_ctx_data_t *)data;

#ifdef HAVE_EGL
    egl_set_swap_interval(&ctx_orbis->egl, 0);
#endif
}

static void orbis_ctx_swap_buffers(void *data, void *data2)
{
    orbis_ctx_data_t *ctx_orbis = (orbis_ctx_data_t *)data;

#ifdef HAVE_EGL
    egl_swap_buffers(&ctx_orbis->egl);
#endif
}

static gfx_ctx_proc_t orbis_ctx_get_proc_address(const char *symbol)
{
#ifdef HAVE_EGL
    return egl_get_proc_address(symbol);
#endif
}

static void orbis_ctx_bind_hw_render(void *data, bool enable)
{
    orbis_ctx_data_t *ctx_orbis = (orbis_ctx_data_t *)data;

#ifdef HAVE_EGL
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

static void orbis_ctx_set_flags(void *data, uint32_t flags)
{
    (void)data;
}

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
    orbis_ctx_get_proc_address,
    NULL,
    NULL,
    NULL,
    "orbis",
    orbis_ctx_get_flags,
    orbis_ctx_set_flags,
    orbis_ctx_bind_hw_render,
    NULL,
    NULL};

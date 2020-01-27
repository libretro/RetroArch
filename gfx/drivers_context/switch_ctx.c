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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <switch.h>

#include "../common/switch_common.h"
#include "../../frontend/frontend_driver.h"

static enum gfx_ctx_api ctx_nx_api = GFX_CTX_OPENGL_API;
switch_ctx_data_t *nx_ctx_ptr = NULL;

extern bool platform_switch_has_focus;

void switch_ctx_destroy(void *data)
{
    switch_ctx_data_t *ctx_nx = (switch_ctx_data_t *)data;

    if (ctx_nx)
    {
#ifdef HAVE_EGL
        egl_destroy(&ctx_nx->egl);
#endif
        ctx_nx->resize = false;
        free(ctx_nx);
    }
}

static void switch_ctx_get_video_size(void *data,
                                      unsigned *width, unsigned *height)
{
   switch_ctx_data_t *ctx_nx = (switch_ctx_data_t *)data;

   switch (appletGetOperationMode())
      {
         default:
         case AppletOperationMode_Handheld:
            *width = 1280;
            *height = 720;
            break;
         case AppletOperationMode_Docked:
            *width = 1920;
            *height = 1080;
            break;
      }
}

static void *switch_ctx_init(video_frame_info_t *video_info, void *video_driver)
{
#ifdef HAVE_EGL
    EGLint n;
    EGLint major, minor;
    static const EGLint attribs[] = {
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE};
#endif

    switch_ctx_data_t *ctx_nx = (switch_ctx_data_t *)calloc(1, sizeof(*ctx_nx));

    if (!ctx_nx)
        return NULL;

    nx_ctx_ptr = ctx_nx;

    /* Comment below to enable error checking */
    setenv("MESA_NO_ERROR", "1", 1);

#if 0
    /* Uncomment below to enable Mesa logging: */
    setenv("EGL_LOG_LEVEL", "debug", 1);
    setenv("MESA_VERBOSE", "all", 1);
    setenv("NOUVEAU_MESA_DEBUG", "1", 1);

    /* Uncomment below to enable shader debugging in Nouveau: */
    setenv("NV50_PROG_OPTIMIZE", "0", 1);
    setenv("NV50_PROG_DEBUG", "1", 1);
    setenv("NV50_PROG_CHIPSET", "0x120", 1);
#endif

    // Needs to be here
   ctx_nx->win = nwindowGetDefault();
   nwindowSetDimensions(ctx_nx->win, 1920, 1080);

#ifdef HAVE_EGL
    if (!egl_init_context(&ctx_nx->egl, EGL_NONE, EGL_DEFAULT_DISPLAY,
                          &major, &minor, &n, attribs, NULL))
    {
        egl_report_error();
        goto error;
    }
#endif

    return ctx_nx;

error:
    printf("[NXGL]: EGL error: %d.\n", eglGetError());
    switch_ctx_destroy(video_driver);
    return NULL;
}

static void switch_ctx_check_window(void *data, bool *quit,
                                    bool *resize, unsigned *width, unsigned *height, bool is_shutdown)
{
    unsigned new_width, new_height;

    switch_ctx_get_video_size(data, &new_width, &new_height);

    if (new_width != *width || new_height != *height)
    {
        *width = new_width;
        *height = new_height;
        switch_ctx_data_t *ctx_nx = (switch_ctx_data_t *)data;

        ctx_nx->width = *width;
        ctx_nx->height = *height;

        ctx_nx->native_window.width = ctx_nx->width;
        ctx_nx->native_window.height = ctx_nx->height;
        ctx_nx->resize = true;

        *resize = true;
        printf("[NXGL]: Resizing to %dx%d\n", *width, *height);
        nwindowSetCrop(ctx_nx->win, 0, 1080 - ctx_nx->height, ctx_nx->width, 1080);
    }

    *quit = (bool)false;
}

static bool switch_ctx_set_video_mode(void *data,
                                      video_frame_info_t *video_info,
                                      unsigned width, unsigned height,
                                      bool fullscreen)
{
    /* Create an EGL rendering context */
    static const EGLint contextAttributeList[] =
        {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE};

    switch_ctx_data_t *ctx_nx = (switch_ctx_data_t *)data;

    switch_ctx_get_video_size(data, &ctx_nx->width, &ctx_nx->height);

    ctx_nx->native_window.width = ctx_nx->width;
    ctx_nx->native_window.height = ctx_nx->height;

    ctx_nx->refresh_rate = 60;

#ifdef HAVE_EGL
    if (!egl_create_context(&ctx_nx->egl, contextAttributeList))
    {
        egl_report_error();
        goto error;
    }
#endif

#ifdef HAVE_EGL
    if (!egl_create_surface(&ctx_nx->egl, ctx_nx->win))
        goto error;
#endif

    nwindowSetCrop(ctx_nx->win, 0, 1080 - ctx_nx->height, ctx_nx->width, 1080);

    return true;

error:
    printf("[NXGL]: EGL error: %d.\n", eglGetError());
    switch_ctx_destroy(data);

    return false;
}

static void switch_ctx_input_driver(void *data,
      const char *name,
      input_driver_t **input, void **input_data)
{
    *input      = NULL;
    *input_data = NULL;
}

static enum gfx_ctx_api switch_ctx_get_api(void *data)
{
    return ctx_nx_api;
}

static bool switch_ctx_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
    (void)data;
    ctx_nx_api = api;

    if (api == GFX_CTX_OPENGL_API)
        if (egl_bind_api(EGL_OPENGL_API))
            return true;

    return false;
}

static bool switch_ctx_has_focus(void *data)
{
    (void)data;
    return platform_switch_has_focus;
}

static bool switch_ctx_suppress_screensaver(void *data, bool enable)
{
    (void)data;
    (void)enable;
    return false;
}

static void switch_ctx_set_swap_interval(void *data,
                                         int swap_interval)
{
    switch_ctx_data_t *ctx_nx = (switch_ctx_data_t *)data;

#ifdef HAVE_EGL
    egl_set_swap_interval(&ctx_nx->egl, swap_interval);
#endif
}

static void switch_ctx_swap_buffers(void *data, void *data2)
{
    switch_ctx_data_t *ctx_nx = (switch_ctx_data_t *)data;

#ifdef HAVE_EGL
    egl_swap_buffers(&ctx_nx->egl);
#endif
}

static gfx_ctx_proc_t switch_ctx_get_proc_address(const char *symbol)
{
#ifdef HAVE_EGL
    return egl_get_proc_address(symbol);
#endif
}

static void switch_ctx_bind_hw_render(void *data, bool enable)
{
    switch_ctx_data_t *ctx_nx = (switch_ctx_data_t *)data;

#ifdef HAVE_EGL
    egl_bind_hw_render(&ctx_nx->egl, enable);
#endif
}

static uint32_t switch_ctx_get_flags(void *data)
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
#ifdef HAVE_GLSL
      BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);
#endif
   }

    return flags;
}

static void switch_ctx_set_flags(void *data, uint32_t flags)
{
    (void)data;
}

static float switch_ctx_get_refresh_rate(void *data)
{
    switch_ctx_data_t *ctx_nx = (switch_ctx_data_t *)data;

    return ctx_nx->refresh_rate;
}

bool switch_ctx_get_metrics(void *data,
   enum display_metric_types type, float *value)
{
   switch (type)
   {
      case DISPLAY_METRIC_DPI:
         *value = 236.87; /* FIXME: Don't hardcode this value */
         return true;
      default:
         break;
   }

   return false;
}

const gfx_ctx_driver_t switch_ctx = {
    switch_ctx_init,
    switch_ctx_destroy,
    switch_ctx_get_api,
    switch_ctx_bind_api,
    switch_ctx_set_swap_interval,
    switch_ctx_set_video_mode,
    switch_ctx_get_video_size,
    switch_ctx_get_refresh_rate,
    NULL, /* get_video_output_size */
    NULL, /* get_video_output_prev */
    NULL, /* get_video_output_next */
    switch_ctx_get_metrics,
    NULL,
    NULL, /* update_title */
    switch_ctx_check_window,
    NULL, /* set_resize */
    switch_ctx_has_focus,
    switch_ctx_suppress_screensaver,
    false, /* has_windowed */
    switch_ctx_swap_buffers,
    switch_ctx_input_driver,
    switch_ctx_get_proc_address,
    NULL,
    NULL,
    NULL,
    "switch",
    switch_ctx_get_flags,
    switch_ctx_set_flags,
    switch_ctx_bind_hw_render,
    NULL,
    NULL};

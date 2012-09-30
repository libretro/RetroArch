/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#include "../../driver.h"

#include <stdint.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <xgraphics.h>

#include "../../screenshot.h"
#include "xdk_ctx.h"

#if defined(_XBOX1)
// for Xbox 1
#define XBOX_PRESENTATIONINTERVAL D3DRS_PRESENTATIONINTERVAL
#else
// for Xbox 360
#define XBOX_PRESENTATIONINTERVAL D3DRS_PRESENTINTERVAL
#endif

static void gfx_ctx_xdk_set_blend(bool enable)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   if(enable)
   {
      d3d->d3d_render_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
      d3d->d3d_render_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   }
   d3d->d3d_render_device->SetRenderState(D3DRS_ALPHABLENDENABLE, enable);
}

static void gfx_ctx_xdk_set_swap_interval(unsigned interval)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   if (interval)
      d3d->d3d_render_device->SetRenderState(XBOX_PRESENTATIONINTERVAL, D3DPRESENT_INTERVAL_ONE);
   else
      d3d->d3d_render_device->SetRenderState(XBOX_PRESENTATIONINTERVAL, D3DPRESENT_INTERVAL_IMMEDIATE);
}

static void gfx_ctx_xdk_get_available_resolutions (void)
{
}


static void gfx_ctx_xdk_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   *quit = false;
   *resize = false;

   if (d3d->quitting)
      *quit = true;

   if (d3d->should_resize)
      *resize = true;
}

static void gfx_ctx_xdk_set_resize(unsigned width, unsigned height) { }

static void gfx_ctx_xdk_swap_buffers(void)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
#ifdef _XBOX1
   d3d->d3d_render_device->EndScene();
#endif
   d3d->d3d_render_device->Present(NULL, NULL, NULL, NULL);
}

static void gfx_ctx_xdk_clear(void)
{
   xdk_d3d_video_t *device_ptr = (xdk_d3d_video_t*)driver.video_data;
#ifdef _XBOX1
   unsigned flicker_filter = g_console.flicker_filter;
   bool soft_filter_enable = g_console.soft_display_filter_enable;
#endif

   device_ptr->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
      D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
#ifdef _XBOX1
   device_ptr->d3d_render_device->BeginScene();
   device_ptr->d3d_render_device->SetFlickerFilter(flicker_filter);
   device_ptr->d3d_render_device->SetSoftDisplayFilter(soft_filter_enable);
#endif
}

static bool gfx_ctx_xdk_window_has_focus(void)
{
   return true;
}

static bool gfx_ctx_xdk_menu_init(void)
{
   return true;
}

static void gfx_ctx_xdk_update_window_title(bool reset) { }

static void gfx_ctx_xdk_get_video_size(unsigned *width, unsigned *height)
{
   /* TODO: implement */
   (void)width;
   (void)height;
}

static bool gfx_ctx_xdk_init(void)
{
   /* TODO: implement */
   return true;
}

static bool gfx_ctx_xdk_set_video_mode(
      unsigned width, unsigned height,
      unsigned bits, bool fullscreen)
{
   /* TODO: implement */
   return true;
}

static void gfx_ctx_xdk_destroy(void)
{
   /* TODO: implement */
}

static void gfx_ctx_xdk_input_driver(const input_driver_t **input, void **input_data) { }

static void gfx_ctx_xdk_set_filtering(unsigned index, bool set_smooth)
{
   /* TODO: implement */
}

static void gfx_ctx_xdk_set_fbo(bool enable)
{
   /* TODO: implement properly */
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   d3d->fbo_enabled = enable;
}

void gfx_ctx_xdk_apply_fbo_state_changes(unsigned mode)
{
}

void gfx_ctx_xdk_screenshot_dump(void *data)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   HRESULT ret = S_OK;
   char filename[PATH_MAX];
   char shotname[PATH_MAX];

   screenshot_generate_filename(shotname, sizeof(shotname));
   snprintf(filename, sizeof(filename), "%s\\%s", default_paths.screenshots_dir, shotname);
   
#if defined(_XBOX1)
   D3DSurface *surf = NULL;
   d3d->d3d_render_device->GetBackBuffer(-1, D3DBACKBUFFER_TYPE_MONO, &surf);
   ret = XGWriteSurfaceToFile(surf, filename);
   surf->Release();
#elif defined(_XBOX360)
   ret = 1; //false
   //ret = D3DXSaveTextureToFile(filename, D3DXIFF_BMP, d3d->lpTexture, NULL);
#endif

   if(ret == S_OK)
   {
      RARCH_LOG("Screenshot saved: %s.\n", filename);
      msg_queue_push(g_extern.msg_queue, "Screenshot saved.", 1, 30);
   }
}

/*============================================================
	MISC
        TODO: Refactor
============================================================ */

void gfx_ctx_set_projection(xdk_d3d_video_t *d3d, const struct gl_ortho *ortho, bool allow_rotate) { }

void gfx_ctx_set_aspect_ratio(void *data, unsigned aspectratio_index)
{
   (void)data;
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   if(g_console.aspect_ratio_index == ASPECT_RATIO_AUTO)
      rarch_set_auto_viewport(g_extern.frame_cache.width, g_extern.frame_cache.height);
   else if(g_console.aspect_ratio_index == ASPECT_RATIO_CORE)
      rarch_set_core_viewport();

   g_settings.video.aspect_ratio = aspectratio_lut[g_console.aspect_ratio_index].value;
   g_settings.video.force_aspect = false;
   d3d->should_resize = true;
}

void gfx_ctx_set_overscan(void)
{
   /* TODO: implement */
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   if (!d3d)
      return;

   d3d->should_resize = true;
}

int gfx_ctx_xdk_check_resolution(unsigned resolution_id)
{
   /* TODO: implement */
   return 0;
}

static bool gfx_ctx_xdk_bind_api(enum gfx_ctx_api api)
{
#if defined(_XBOX1)
   return api == GFX_CTX_DIRECT3D8_API;
#elif defined(_XBOX360)
   return api == GFX_CTX_DIRECT3D9_API;
#endif
}


const gfx_ctx_driver_t gfx_ctx_xdk = {
   gfx_ctx_xdk_init,
   gfx_ctx_xdk_destroy,
   gfx_ctx_xdk_bind_api,
   gfx_ctx_xdk_set_swap_interval,
   gfx_ctx_xdk_set_video_mode,
   gfx_ctx_xdk_get_video_size,
   NULL,
   gfx_ctx_xdk_update_window_title,
   gfx_ctx_xdk_check_window,
   gfx_ctx_xdk_set_resize,
   gfx_ctx_xdk_window_has_focus,
   gfx_ctx_xdk_swap_buffers,
   gfx_ctx_xdk_input_driver,
   NULL,
   "xdk",

   // RARCH_CONSOLE stuff.
   gfx_ctx_xdk_set_filtering,
   gfx_ctx_xdk_get_available_resolutions,
   gfx_ctx_xdk_check_resolution,

   gfx_ctx_xdk_menu_init,

   gfx_ctx_xdk_set_fbo,
   gfx_ctx_xdk_apply_fbo_state_changes,
};

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

#include "xdk_ctx.h"

#if defined(_XBOX1)
// for Xbox 1
#define XBOX_PRESENTATIONINTERVAL D3DRS_PRESENTATIONINTERVAL
#else
// for Xbox 360
#define XBOX_PRESENTATIONINTERVAL D3DRS_PRESENTINTERVAL
#endif

void gfx_ctx_set_blend(bool enable)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   if(enable)
   {
      d3d->d3d_render_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
      d3d->d3d_render_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   }
   d3d->d3d_render_device->SetRenderState(D3DRS_ALPHABLENDENABLE, enable);
}

void gfx_ctx_set_swap_interval(unsigned interval, bool inited)
{
   (void)inited;
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   if (interval)
      d3d->d3d_render_device->SetRenderState(XBOX_PRESENTATIONINTERVAL, D3DPRESENT_INTERVAL_ONE);
   else
      d3d->d3d_render_device->SetRenderState(XBOX_PRESENTATIONINTERVAL, D3DPRESENT_INTERVAL_IMMEDIATE);
}

void gfx_ctx_check_window(bool *quit,
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

void gfx_ctx_set_resize(unsigned width, unsigned height) { }

void gfx_ctx_swap_buffers(void)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
#ifdef _XBOX1
   d3d->d3d_render_device->EndScene();
#endif
   d3d->d3d_render_device->Present(NULL, NULL, NULL, NULL);
}

void gfx_ctx_clear(void)
{
   xdk_d3d_video_t *device_ptr = (xdk_d3d_video_t*)driver.video_data;
   device_ptr->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
      D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
#ifdef _XBOX1
   device_ptr->d3d_render_device->BeginScene();
   device_ptr->d3d_render_device->SetFlickerFilter(1);
   device_ptr->d3d_render_device->SetSoftDisplayFilter(false);
#endif
}

#ifndef HAVE_GRIFFIN
bool gfx_ctx_window_has_focus(void)
{
   return true;
}
#endif

bool gfx_ctx_menu_init(void)
{
   return true;
}

void gfx_ctx_update_window_title(bool reset) { }

void gfx_ctx_get_video_size(unsigned *width, unsigned *height)
{
   (void)width;
   (void)height;
}

bool gfx_ctx_init(void)
{
   return true;
}

bool gfx_ctx_set_video_mode(
      unsigned width, unsigned height,
      unsigned bits, bool fullscreen)
{
   return true;
}

void gfx_ctx_destroy(void)
{
}

void gfx_ctx_input_driver(const input_driver_t **input, void **input_data) { }

void gfx_ctx_set_filtering(unsigned index, bool set_smooth) { }

void gfx_ctx_set_fbo(bool enable)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   d3d->fbo_enabled = enable;
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

   g_settings.video.aspect_ratio = aspectratio_lut[g_console.aspect_ratio_index].value;
   g_settings.video.force_aspect = false;
   d3d->should_resize = true;
}

void gfx_ctx_set_overscan(void)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   if (!d3d)
      return;

   d3d->should_resize = true;
}

int gfx_ctx_check_resolution(unsigned resolution_id)
{
   return 0;
}

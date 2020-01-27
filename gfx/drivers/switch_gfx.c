/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018      - misson20000
 *  Copyright (C) 2018      - m4xw
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

#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <retro_inline.h>
#include <retro_math.h>
#include <formats/image.h>

#include <gfx/scaler/scaler.h>
#include <gfx/scaler/pixconv.h>

#include <libtransistor/nx.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../font_driver.h"

#include "../../configuration.h"
#include "../../command.h"
#include "../../driver.h"

#include "../../retroarch.h"
#include "../../verbosity.h"

#ifndef HAVE_THREADS
#include "../../tasks/tasks_internal.h"
#endif

typedef struct
{
   bool vsync;
   bool rgb32;
   unsigned width, height;
   unsigned rotation;
   struct video_viewport vp;

	struct {
		bool enable;
		bool fullscreen;

		uint32_t *pixels;

		unsigned width;
		unsigned height;

		unsigned tgtw;
		unsigned tgth;

		struct scaler_ctx scaler;
	} menu_texture;

	surface_t surface;
	revent_h vsync_h;
	uint32_t image[1280*720];

	struct scaler_ctx scaler;
	uint32_t last_width;
	uint32_t last_height;
} switch_video_t;

static void *switch_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   unsigned x, y;
   switch_video_t *sw = (switch_video_t*)calloc(1, sizeof(*sw));
   if (!sw)
      return NULL;

   RARCH_LOG("loading switch gfx driver, width: %d, height: %d\n", video->width, video->height);

   result_t r = display_init();
   if (r != RESULT_OK)
   {
      free(sw);
      return NULL;
   }
   r = display_open_layer(&sw->surface);

   if (r != RESULT_OK)
   {
      display_finalize();
      free(sw);
      return NULL;
   }
   r = display_get_vsync_event(&sw->vsync_h);

   if (r != RESULT_OK)
   {
	   display_close_layer(&sw->surface);
      display_finalize();
      free(sw);
      return NULL;
   }

   sw->vp.x           = 0;
   sw->vp.y           = 0;
   sw->vp.width       = 1280;
   sw->vp.height      = 720;
   sw->vp.full_width  = 1280;
   sw->vp.full_height = 720;
   video_driver_set_size(&sw->vp.width, &sw->vp.height);

   sw->vsync = video->vsync;
   sw->rgb32 = video->rgb32;

   *input = NULL;
   *input_data = NULL;

   return sw;
}

static void switch_wait_vsync(switch_video_t *sw)
{
	uint32_t handle_idx;
	svcWaitSynchronization(&handle_idx, &sw->vsync_h, 1, 33333333);
	svcResetSignal(sw->vsync_h);
}

static bool switch_frame(void *data, const void *frame,
      unsigned width, unsigned height,
      uint64_t frame_count, unsigned pitch,
      const char *msg, video_frame_info_t *video_info)
{
	static uint64_t last_frame = 0;

   unsigned x, y;
   result_t r;
   int tgtw, tgth, centerx, centery;
   uint32_t *out_buffer   = NULL;
   switch_video_t *sw     = data;
   int xsf                = 1280 / width;
   int ysf                = 720  / height;
   int sf                 = xsf;

   if (ysf < sf)
      sf = ysf;

   tgtw                   = width * sf;
   tgth                   = height * sf;
   centerx                = (1280-tgtw)/2;
   centery                = (720-tgth)/2;

   /* clear image to black */
   for(y = 0; y < 720; y++)
   {
      for(x = 0; x < 1280; x++)
         sw->image[y*1280+x] = 0xFF000000;
   }

   if(width > 0 && height > 0)
   {
	   if(sw->last_width != width ||
	      sw->last_height != height)
      {
         scaler_ctx_gen_reset(&sw->scaler);

         sw->scaler.in_width    = width;
         sw->scaler.in_height   = height;
         sw->scaler.in_stride   = pitch;
         sw->scaler.in_fmt      = sw->rgb32 ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGB565;

         sw->scaler.out_width   = tgtw;
         sw->scaler.out_height  = tgth;
         sw->scaler.out_stride  = 1280 * sizeof(uint32_t);
         sw->scaler.out_fmt     = SCALER_FMT_ABGR8888;

         sw->scaler.scaler_type = SCALER_TYPE_POINT;

         if(!scaler_ctx_gen_filter(&sw->scaler))
         {
            RARCH_ERR("failed to generate scaler for main image\n");
            return false;
         }

         sw->last_width         = width;
         sw->last_height        = height;
      }

	   scaler_ctx_scale(&sw->scaler, sw->image + (centery * 1280) + centerx, frame);
   }

#if defined(HAVE_MENU)
   if (sw->menu_texture.enable)
	{
		menu_driver_frame(video_info);

		if (sw->menu_texture.pixels)
		{
#if 0
			if (sw->menu_texture.fullscreen)
         {
#endif
	         scaler_ctx_scale(&sw->menu_texture.scaler, sw->image +
	                          ((720-sw->menu_texture.tgth)/2)*1280 +
	                          ((1280-sw->menu_texture.tgtw)/2), sw->menu_texture.pixels);
#if 0
         }
         else
         {
         }
#endif
		}
	}
   else if (video_info->statistics_show)
   {
      struct font_params *osd_params = (struct font_params*)
         &video_info->osd_stat_params;

      if (osd_params)
         font_driver_render_msg(sw, video_info, video_info->stat_text,
               (const struct font_params*)&video_info->osd_stat_params, NULL);
   }
#endif

#if 0
   if (frame_count > 6000)
   {
      display_finalize();
      exit(0);
   }
#endif

   if (msg && strlen(msg) > 0)
      RARCH_LOG("message: %s\n", msg);

   r = surface_dequeue_buffer(&sw->surface, &out_buffer);
   if(r != RESULT_OK)
	   return true; /* just skip the frame */

   r = surface_wait_buffer(&sw->surface);
   if(r != RESULT_OK)
	   return true;
   gfx_slow_swizzling_blit(out_buffer, sw->image, 1280, 720, 0, 0);

   r = surface_queue_buffer(&sw->surface);

   if (r != RESULT_OK)
      return false;

   last_frame = svcGetSystemTick();
   return true;
}

static void switch_set_nonblock_state(void *data, bool toggle)
{
   switch_video_t *sw = data;
   sw->vsync = !toggle;
}

static bool switch_alive(void *data)
{
	(void) data;
	return true;
}

static bool switch_focus(void *data)
{
	(void) data;
	return true;
}

static bool switch_suppress_screensaver(void *data, bool enable)
{
	(void) data;
	(void) enable;
	return false;
}

static bool switch_has_windowed(void *data)
{
	(void) data;
	return false;
}

static void switch_free(void *data)
{
	switch_video_t *sw = data;
	svcCloseHandle(sw->vsync_h);
	display_close_layer(&sw->surface);
	display_finalize();
	free(sw);
}

static bool switch_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void) data;
   (void) type;
   (void) path;

   return false;
}

static void switch_set_rotation(void *data, unsigned rotation)
{
   switch_video_t *sw = data;
   if (!sw)
      return;
   sw->rotation = rotation;
}

static void switch_viewport_info(void *data, struct video_viewport *vp)
{
   switch_video_t *sw = data;
   *vp = sw->vp;
}

static void switch_set_texture_frame(
      void *data, const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   switch_video_t *sw = data;

   if (  !sw->menu_texture.pixels         ||
         sw->menu_texture.width  != width ||
         sw->menu_texture.height != height)
   {
      struct scaler_ctx *sctx;
      int xsf, ysf, sf;
      if (sw->menu_texture.pixels)
         free(sw->menu_texture.pixels);

      sw->menu_texture.pixels = malloc(width * height * (rgb32 ? 4 : 2));
      if (!sw->menu_texture.pixels)
      {
         RARCH_ERR("failed to allocate buffer for menu texture\n");
         return;
      }

      xsf                     = 1280 / width;
      ysf                     = 720  / height;
      sf                      = xsf;

      if (ysf < sf)
	      sf = ysf;

      sw->menu_texture.width  = width;
      sw->menu_texture.height = height;
      sw->menu_texture.tgtw   = width * sf;
      sw->menu_texture.tgth   = height * sf;

      sctx                    = &sw->menu_texture.scaler;
      scaler_ctx_gen_reset(sctx);

      sctx->in_width          = width;
      sctx->in_height         = height;
      sctx->in_stride         = width * (rgb32 ? 4 : 2);
      sctx->in_fmt            = rgb32 ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGB565;

      sctx->out_width         = sw->menu_texture.tgtw;
      sctx->out_height        = sw->menu_texture.tgth;
      sctx->out_stride        = 1280 * 4;
      sctx->out_fmt           = SCALER_FMT_ABGR8888;

      sctx->scaler_type       = SCALER_TYPE_POINT;

      if (!scaler_ctx_gen_filter(sctx))
      {
         RARCH_ERR("failed to generate scaler for menu texture\n");
         return;
      }
   }

   memcpy(sw->menu_texture.pixels, frame, width * height * (rgb32 ? 4 : 2));
}

static void switch_set_texture_enable(void *data, bool enable, bool full_screen)
{
	switch_video_t *sw = data;
   if (!sw)
      return;

	sw->menu_texture.enable = enable;
	sw->menu_texture.fullscreen = full_screen;
}

static const video_poke_interface_t switch_poke_interface = {
   NULL, /* get_flags */
	NULL, /* load_texture */
	NULL, /* unload_texture */
	NULL, /* set_video_mode */
	NULL, /* get_refresh_rate */
	NULL, /* set_filtering */
	NULL, /* get_video_output_size */
	NULL, /* get_video_output_prev */
	NULL, /* get_video_output_next */
	NULL, /* get_current_framebuffer */
	NULL, /* get_proc_address */
	NULL, /* set_aspect_ratio */
	NULL, /* apply_state_changes */
	switch_set_texture_frame,
	switch_set_texture_enable,
	NULL, /* set_osd_msg */
	NULL, /* show_mouse */
	NULL, /* grab_mouse_toggle */
	NULL, /* get_current_shader */
	NULL, /* get_current_software_framebuffer */
	NULL, /* get_hw_render_interface */
};

static void switch_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void) data;
   *iface = &switch_poke_interface;
}

video_driver_t video_switch = {
	switch_init,
	switch_frame,
	switch_set_nonblock_state,
	switch_alive,
	switch_focus,
	switch_suppress_screensaver,
	switch_has_windowed,
	switch_set_shader,
	switch_free,
	"switch",
	NULL, /* set_viewport */
	switch_set_rotation,
	switch_viewport_info,
	NULL, /* read_viewport  */
	NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
	NULL, /* overlay_interface */
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
	switch_get_poke_interface,
};

/* vim: set ts=3 sw=3 */

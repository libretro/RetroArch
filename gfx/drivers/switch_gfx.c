#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <retro_inline.h>
#include <retro_math.h>
#include <formats/image.h>

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
   unsigned width, height;
   unsigned rotation;
   surface_t surface;
   revent_h vsync_h;
   struct video_viewport vp;
} switch_video_t;

static uint32_t image[1280*720];

static void *switch_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   unsigned x, y;
   switch_video_t *sw = malloc(sizeof(*sw));
   if (!sw)
      return NULL;

   RARCH_LOG("loading switch gfx driver, width: %d, height: %d\n", video->width, video->height);

   if (libtransistor_context.magic != LIBTRANSISTOR_CONTEXT_MAGIC)
      RARCH_LOG("running under CTU, skipping graphics init...\n");
   else
   {
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
         display_finalize();
         free(sw);
         return NULL;
      }
   }

   sw->vp.x           = 0;
   sw->vp.y           = 0;
   sw->vp.width       = 1280;
   sw->vp.height      = 720;
   sw->vp.full_width  = 1280;
   sw->vp.full_height = 720;
   video_driver_set_size(&sw->vp.width, &sw->vp.height);

   sw->vsync = video->vsync;

   *input = NULL;
   *input_data = NULL;

   for(x = 0; x < 1280; x++)
   {
      for(y = 0; y < 720; y++)
         image[(y*1280)+x] = 0xFF000000;
   }

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
   unsigned x, y;
   result_t r;
   uint64_t begin, done_copying, post_vsync, pre_swizzle, post_swizzle,
            copy_ms, swizzle_ms, vsync_ms;
   int tgtw, tgth, centerx, centery;
   const uint16_t *frame_pixels = frame;
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

   begin                  = svcGetSystemTick();

   for(x = 0; x < width; x++)
   {
      for(y = 0; y < height; y++)
      {
         uint32_t spixel = frame_pixels[(y*pitch/sizeof(uint16_t)) + x];
         uint32_t dpixel = 0;
         uint8_t r       = (spixel >> 11) & 31;
         uint8_t g       = (spixel >> 5) & 63;
         uint8_t b       = (spixel >> 0) & 31;
         r               = (r * 256) / 32;
         g               = (g * 256) / 64;
         b               = (b * 256) / 32;
         dpixel          = (r << 0) | (g << 8) | (b << 16) | (0xFF << 24);

         for (int subx = 0; subx < xsf; subx++)
            for (int suby = 0; suby < ysf; suby++)
               image[(((y*sf)+suby+centery)*1280) 
                  + ((x*sf)+subx+centerx)] = dpixel;
      }
   }

   done_copying = svcGetSystemTick();

#if 0
   if (frame_count > 6000)
   {
      display_finalize();
      exit(0);
   }
#endif

   if (libtransistor_context.magic != LIBTRANSISTOR_CONTEXT_MAGIC)
   {
      RARCH_LOG("running under CTU; skipping frame\n");
      return true;
   }

   if (msg != NULL && strlen(msg) > 0)
      RARCH_LOG("message: %s\n", msg);

   if (sw->vsync)
      switch_wait_vsync(sw);

   post_vsync = svcGetSystemTick();

   r = surface_dequeue_buffer(&sw->surface, &out_buffer);

   if (r != RESULT_OK)
      return false;

   pre_swizzle  = svcGetSystemTick();
   gfx_slow_swizzling_blit(out_buffer, image, 1280, 720, 0, 0);
   post_swizzle = svcGetSystemTick();

   r = surface_queue_buffer(&sw->surface);

   if (r != RESULT_OK)
      return false;

   copy_ms    = (done_copying - begin) / 19200;
   swizzle_ms = (post_swizzle - pre_swizzle) / 19200;
   vsync_ms   = (post_vsync - done_copying) / 19200;

   RARCH_LOG("frame %d benchmark: copy %ld ms, swizzle %ld ms, vsync %ld ms\n", frame_count, copy_ms, swizzle_ms, vsync_ms);

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
	free(sw);
	display_finalize();
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

static bool switch_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   (void) data;
   (void) buffer;

   return true;
}

static void switch_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void) data;
   (void) iface;
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
	switch_read_viewport,
	NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
	NULL, /* overlay_interface */
#endif
	switch_get_poke_interface,
};

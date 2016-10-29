/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2016 - Ali Bouhlel
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
#include "../../configuration.h"
#include "../../verbosity.h"

#include <coreinit/screen.h>
#include <coreinit/cache.h>
#include "system/memory.h"
#include "string.h"

#include "wiiu_dbg.h"

typedef struct
{
   void* screen_buffer0;
   int screen_buffer0_size;
   void* screen_buffer1;
   int screen_buffer1_size;
   int screen_buffer0_id;
   int screen_buffer1_id;
   struct
   {
      void* texture;
      int tex_width;
      int tex_height;
      int width;
      int height;
      bool enable;
   } menu;

   void* texture;
   int tex_width;
   int tex_height;

} wiiu_video_t;

static void* wiiu_gfx_init(const video_info_t* video,
                           const input_driver_t** input, void** input_data)
{
   *input = NULL;
   *input_data = NULL;

   wiiu_video_t* wiiu = calloc(1, sizeof(*wiiu));

   if (!wiiu)
      return NULL;

   wiiu->screen_buffer0_size = OSScreenGetBufferSizeEx(0);
   wiiu->screen_buffer0      = MEM1_alloc(wiiu->screen_buffer0_size, 0x40);
   wiiu->screen_buffer1_size = OSScreenGetBufferSizeEx(1);
   wiiu->screen_buffer1      = MEM1_alloc(wiiu->screen_buffer1_size, 0x40);

   DEBUG_INT(wiiu->screen_buffer0_size);
   DEBUG_INT(wiiu->screen_buffer1_size);

   OSScreenSetBufferEx(0, wiiu->screen_buffer0);
   OSScreenSetBufferEx(1, wiiu->screen_buffer1);
   OSScreenEnableEx(0, 1);
   OSScreenEnableEx(1, 1);
   OSScreenClearBufferEx(0, 0);
   OSScreenClearBufferEx(1, 0);

   DCFlushRange(wiiu->screen_buffer0, wiiu->screen_buffer0_size);
   DCFlushRange(wiiu->screen_buffer1, wiiu->screen_buffer1_size);

   OSScreenFlipBuffersEx(0);
   wiiu->screen_buffer0_id = 0;
   OSScreenFlipBuffersEx(1);
   wiiu->screen_buffer1_id = 0;

   wiiu->menu.tex_width = 512;
   wiiu->menu.tex_height = 512;
   wiiu->menu.texture = malloc(wiiu->menu.tex_width * wiiu->menu.tex_height * sizeof(uint16_t));

   wiiu->tex_width = video->input_scale * RARCH_SCALE_BASE;;
   wiiu->tex_height = video->input_scale * RARCH_SCALE_BASE;;
   wiiu->texture = malloc(wiiu->tex_width * wiiu->tex_height * sizeof(uint16_t));


   if (input && input_data)
   {
      void* wiiuinput   = NULL;
      wiiuinput = input_wiiu.init();
      *input = wiiuinput ? &input_wiiu : NULL;
      *input_data = wiiuinput;
   }

   DEBUG_LINE();

   return wiiu;
}
static void wiiu_gfx_free(void* data)
{
   wiiu_video_t* wiiu = (wiiu_video_t*) data;

   if (!wiiu)
      return;

   MEM1_free(wiiu->screen_buffer0);
   MEM1_free(wiiu->screen_buffer1);
   free(wiiu->menu.texture);
   free(wiiu->texture);
   free(wiiu);
   DEBUG_LINE();

}

static bool wiiu_gfx_frame(void* data, const void* frame,
                           unsigned width, unsigned height, uint64_t frame_count,
                           unsigned pitch, const char* msg)
{
   (void)frame;
   (void)width;
   (void)height;
   (void)pitch;
   (void)msg;
   int i;

   wiiu_video_t* wiiu = (wiiu_video_t*) data;

   static int frames = 0;
   char frames_str [512];
   snprintf(frames_str, sizeof(frames_str), "frames : %i", frames++);

   OSScreenClearBufferEx(1, 0);

   if (wiiu->menu.enable)
   {
      const uint16_t* src = (uint16_t*)wiiu->menu.texture;
      uint32_t* dst = (uint32_t*)((uint8_t*)wiiu->screen_buffer1 + wiiu->screen_buffer1_id * wiiu->screen_buffer1_size / 2);

      dst += 896 * (480 - wiiu->menu.height) / 2 + (896 - wiiu->menu.width) / 2;
      int x, y;

      for (y = 0; y < wiiu->menu.height; y++)
      {
         for (x = 0; x < wiiu->menu.width; x++)
         {
            int r = ((src[x] >> 12) & 0xF) << 4;
            int g = ((src[x] >>  8) & 0xF) << 4;
            int b = ((src[x] >>  4) & 0xF) << 4;
            dst[x] = (r << 0) | (b << 8) | (g << 16);
         }

         src += wiiu->menu.tex_width;
         dst += 896;
      }

   }
   else
   {
      const uint16_t* src = (uint16_t*)frame;
      uint32_t* dst = (uint32_t*)((uint8_t*)wiiu->screen_buffer1 + wiiu->screen_buffer1_id * wiiu->screen_buffer1_size / 2);

      dst += (896 * (480 - height) + width) / 2;
      int x, y;

      for (y = 0; y < height; y++)
      {
         for (x = 0; x < width; x++)
         {
            int r = ((src[x] >> 11) & 0x1F) << 3;
            int g = ((src[x] >>  5) & 0x3F) << 2;
            int b = ((src[x] >>  0) & 0x1F) << 3;
            dst[x] = (r << 0) | (b << 8) | (g << 16);
         }

         src += pitch/2;
         dst += 896;
      }
   }



   OSScreenPutFontEx(1, 0, 16, frames_str);
   DCFlushRange(((uint8_t*)wiiu->screen_buffer1 + wiiu->screen_buffer1_id * wiiu->screen_buffer1_size / 2)
                , wiiu->screen_buffer1_size / 2);
   OSScreenFlipBuffersEx(1);
   wiiu->screen_buffer1_id ^= 1;



   return true;
}

static void wiiu_gfx_set_nonblock_state(void* data, bool toggle)
{
   (void)data;
   (void)toggle;
}

static bool wiiu_gfx_alive(void* data)
{
   (void)data;
   return true;
}

static bool wiiu_gfx_focus(void* data)
{
   (void)data;
   return true;
}

static bool wiiu_gfx_suppress_screensaver(void* data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool wiiu_gfx_has_windowed(void* data)
{
   (void)data;
   return true;
}

static bool wiiu_gfx_set_shader(void* data,
                                enum rarch_shader_type type, const char* path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void wiiu_gfx_set_rotation(void* data,
                                  unsigned rotation)
{
   (void)data;
   (void)rotation;
}

static void wiiu_gfx_viewport_info(void* data,
                                   struct video_viewport* vp)
{
   (void)data;
   (void)vp;
}

static bool wiiu_gfx_read_viewport(void* data, uint8_t* buffer)
{
   (void)data;
   (void)buffer;

   return true;
}

static uintptr_t wiiu_load_texture(void* video_data, void* data,
                                   bool threaded, enum texture_filter_type filter_type)
{
   return 0;
}
static void wiiu_unload_texture(void* data, uintptr_t handle)
{

}
static void wiiu_set_filtering(void* data, unsigned index, bool smooth)
{
}
static void wiiu_set_aspect_ratio(void* data, unsigned aspect_ratio_idx)
{

}
static void wiiu_apply_state_changes(void* data)
{

}

static void wiiu_viewport_info(void* data, struct video_viewport* vp)
{
   vp->full_width = 800;
   vp->full_height = 480;
   vp->width = 800;
   vp->height = 480;
   vp->x = 0;
   vp->y = 0;
}
static void wiiu_set_texture_frame(void* data, const void* frame, bool rgb32,
                                   unsigned width, unsigned height, float alpha)
{
   int i;
   wiiu_video_t* wiiu = (wiiu_video_t*) data;

   if (!wiiu)
      return;

   if (!frame || !width || !height)
      return;

   if (width > wiiu->menu.tex_width)
      width = wiiu->menu.tex_width;

   if (height > wiiu->menu.tex_height)
      height = wiiu->menu.tex_height;

   wiiu->menu.width = width;
   wiiu->menu.height = height;

   const uint16_t* src = frame;
   uint16_t* dst = (uint16_t*)wiiu->menu.texture;

   for (i = 0; i < height; i++)
   {
      memcpy(dst, src, width * sizeof(uint16_t));
      dst += wiiu->menu.tex_width;
      src += width;
   }
}

static void wiiu_set_texture_enable(void* data, bool state, bool full_screen)
{
   (void) full_screen;
   wiiu_video_t* wiiu = (wiiu_video_t*) data;
   wiiu->menu.enable = state;

}

static void wiiu_set_osd_msg(void* data, const char* msg,
                             const struct font_params* params, void* font)
{
}



static const video_poke_interface_t wiiu_poke_interface =
{
   wiiu_load_texture,
   wiiu_unload_texture,
   NULL,
   wiiu_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL,
   wiiu_set_aspect_ratio,
   wiiu_apply_state_changes,
#ifdef HAVE_MENU
   wiiu_set_texture_frame,
   wiiu_set_texture_enable,
   wiiu_set_osd_msg,
#endif
   NULL,
   NULL,
   NULL
};

static void wiiu_gfx_get_poke_interface(void* data,
                                        const video_poke_interface_t** iface)
{
   (void)data;
   *iface = &wiiu_poke_interface;
}

video_driver_t video_wiiu =
{
   wiiu_gfx_init,
   wiiu_gfx_frame,
   wiiu_gfx_set_nonblock_state,
   wiiu_gfx_alive,
   wiiu_gfx_focus,
   wiiu_gfx_suppress_screensaver,
   wiiu_gfx_has_windowed,
   wiiu_gfx_set_shader,
   wiiu_gfx_free,
   "gx2",
   NULL, /* set_viewport */
   wiiu_gfx_set_rotation,
   wiiu_gfx_viewport_info,
   wiiu_gfx_read_viewport,
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL, /* overlay_interface */
#endif
   wiiu_gfx_get_poke_interface,
};

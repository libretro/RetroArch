/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#include "overlay.h"
#include "../general.h"
#include "../driver.h"
#include "../libretro.h"
#include "../gfx/image.h"

struct input_overlay
{
   void *iface_data;
   const video_overlay_interface_t *iface;
};

input_overlay_t *input_overlay_new(const char *overlay)
{
   (void)overlay;

   input_overlay_t *ol = (input_overlay_t*)calloc(1, sizeof(*ol));
   if (!ol)
      goto error;

   if (!driver.video->overlay_interface)
   {
      RARCH_ERR("Overlay interface is not present in video driver.\n");
      goto error;
   }

   video_overlay_interface_func(&ol->iface);
   ol->iface_data = driver.video_data;

   if (!ol->iface)
      goto error;

   // Test hardcoded.
   struct texture_image img = {0};
   if (!texture_image_load("/tmp/test.png", &img))
   {
      RARCH_ERR("Failed to load overlay image.\n");
      goto error;
   }

   ol->iface->load(ol->iface_data, img.pixels, img.width, img.height);
   free(img.pixels);

   ol->iface->enable(ol->iface_data, true);

   return ol;

error:
   input_overlay_free(ol);
   return NULL;
}

struct overlay_desc
{
   float x;
   float y;
   float rad;
   unsigned key;
};

// TODO: This will be part of a config of some sort.
static const struct overlay_desc descs[] = {
   { 0.25, 0.5, 0.1, RETRO_DEVICE_ID_JOYPAD_LEFT },
   { 0.75, 0.5, 0.1, RETRO_DEVICE_ID_JOYPAD_RIGHT },
};

uint64_t input_overlay_poll(input_overlay_t *ol, int16_t norm_x, int16_t norm_y)
{
   // norm_x and norm_y is in [-0x7fff, 0x7fff] range, like RETRO_DEVICE_POINTER.
   float x = (float)(norm_x + 0x7fff) / 0xffff;
   float y = (float)(norm_y + 0x7fff) / 0xffff;

   uint64_t state = 0;
   for (unsigned i = 0; i < ARRAY_SIZE(descs); i++)
   {
      float sq_dist = (x - descs[i].x) * (x - descs[i].x) + (y - descs[i].y) * (y - descs[i].y);
      if (sq_dist <= descs[i].rad * descs[i].rad)
         state |= UINT64_C(1) << descs[i].key;
   }

   return state;
}

void input_overlay_next(input_overlay_t *ol)
{
   // Dummy
   (void)ol;
}

void input_overlay_free(input_overlay_t *ol)
{
   if (!ol)
      return;

   if (ol->iface)
      ol->iface->enable(ol->iface_data, false);

   free(ol);
}


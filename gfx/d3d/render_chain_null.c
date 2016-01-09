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

#include <string.h>
#include <retro_inline.h>
#include "render_chain_driver.h"

typedef struct null_renderchain
{
   void *empty;
} null_renderchain_t;

static void null_renderchain_free(void *data)
{
}

static void *null_renderchain_new(void)
{
   null_renderchain_t *renderchain = (null_renderchain_t*)calloc(1, sizeof(*renderchain));
   if (!renderchain)
      return NULL;

   return renderchain;
}

static bool null_renderchain_init(void *data,
      const void *info,
      void *dev_data,
      const void *final_viewport_data,
      const void *info_data,
      bool rgb32
      )
{
   (void)data;
   (void)info;
   (void)dev_data;
   (void)final_viewport_data;
   (void)info_data;
   (void)rgb32;

   return true;
}

static void null_renderchain_set_final_viewport(void *data,
      void *renderchain_data, const void *viewport_data)
{
   (void)data;
   (void)renderchain_data;
   (void)viewport_data;
}

static bool null_renderchain_render(void *data, const void *frame,
      unsigned width, unsigned height,
      unsigned pitch, unsigned rotation)
{
   (void)data;
   (void)frame;
   (void)width;
   (void)height;
   (void)pitch;
   (void)rotation;

   return true;
}

static bool null_renderchain_add_lut(void *data,
      const char *id, const char *path, bool smooth)
{
   (void)data;
   (void)id;
   (void)path;
   (void)smooth;

   return true;
}

static bool null_renderchain_add_pass(void *data, const void *info_data)
{
   (void)data;
   (void)info_data;

   return true;
}

static void null_renderchain_add_state_tracker(void *data, void *tracker_data)
{
   (void)data;
   (void)tracker_data;
}

static void null_renderchain_convert_geometry(
	  void *data, const void *info_data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height,
      void *final_viewport_data)
{
   (void)data;
   (void)info_data;
   (void)out_width;
   (void)out_height;
   (void)width;
   (void)height;
   (void)final_viewport_data;
}

renderchain_driver_t null_renderchain = {
   null_renderchain_free,
   null_renderchain_new,
   NULL,
   null_renderchain_init,
   null_renderchain_set_final_viewport,
   null_renderchain_add_pass,
   null_renderchain_add_lut,
   null_renderchain_add_state_tracker,
   null_renderchain_render,
   null_renderchain_convert_geometry,
   NULL,
   NULL,
   NULL,
   "null",
};

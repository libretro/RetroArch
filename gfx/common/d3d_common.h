/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef _D3D_COMMON_H
#define _D3D_COMMON_H

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#if !defined(__WINRT__) && !defined(_XBOX)

#ifndef HAVE_WINDOW
#define HAVE_WINDOW
#endif

#ifndef HAVE_MONITOR
#define HAVE_MONITOR
#endif

#endif

#include <boolean.h>
#include <retro_common_api.h>

#include "../font_driver.h"
#include "../../retroarch.h"

RETRO_BEGIN_DECLS

typedef struct Vertex
{
   float x, y, z;
   uint32_t color;
   float u, v;
} Vertex;

typedef struct
{
   void *tex;
   void *vert_buf;
   /* The quad last written to vert_buf (valid while vert_sent_ok):
    * the draw locks the buffer only when the quad has changed. */
   Vertex vert_sent[4];
   /* The texture's pixel size, packed. */
   unsigned tex_dims;
   float tex_coords[4];
   float vert_coords[4];
   float alpha_mod;
   bool fullscreen;
   bool enabled;
   bool vert_sent_ok;
} overlay_t;

int32_t d3d_translate_filter(unsigned type);

/* Driver Pacing on the Direct3D drivers, through NVAPI's Reflex entry
 * points (NvAPI_D3D_SetSleepMode / NvAPI_D3D_Sleep /
 * NvAPI_D3D_SetLatencyMarker), which take any Direct3D 9 to 12
 * device. nvapi is resolved at runtime by its published interface ids
 * and never linked; a machine without it reports the device reason.
 * One of these per driver instance, zeroed with it; the mode is a
 * property of @device, the device it was set on. The functions follow
 * the driver_pacing_* pokes (gfx/video_driver.h). */
typedef struct d3d_driver_pacing
{
   uint64_t frame_id;
   void *device;
   uint32_t min_interval_us;
   uint8_t mode;      /* enum video_driver_pacing_mode set on @device */
   uint8_t stage;     /* 0 released, 1 rendering, 2 presented */
   bool failed;
} d3d_driver_pacing_t;

unsigned d3d_driver_pacing_set(d3d_driver_pacing_t *s, void *device,
      unsigned mode);
uint64_t d3d_driver_pacing_wait(d3d_driver_pacing_t *s, void *device,
      unsigned min_interval_us, unsigned max_wait_us);
void d3d_driver_pacing_mark(d3d_driver_pacing_t *s, void *device,
      uint64_t frame_id, unsigned marker);
/* The released frame's render submission begins, once; and its
 * present, around the driver's content present, once. */
void d3d_driver_pacing_render_begin(d3d_driver_pacing_t *s, void *device);
void d3d_driver_pacing_present_begin(d3d_driver_pacing_t *s, void *device);
void d3d_driver_pacing_present_end(d3d_driver_pacing_t *s, void *device);
/* The driver goes: the mode is taken off its device. */
void d3d_driver_pacing_free(d3d_driver_pacing_t *s, void *device);

RETRO_END_DECLS

#endif

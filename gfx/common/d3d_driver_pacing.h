/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - Libretro team
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

#ifndef _D3D_DRIVER_PACING_H
#define _D3D_DRIVER_PACING_H

#include <stdint.h>
#include <boolean.h>
#include <retro_atomic.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Driver Pacing on the Direct3D drivers, through NVAPI's Reflex entry
 * points (NvAPI_D3D_SetSleepMode / NvAPI_D3D_Sleep /
 * NvAPI_D3D_SetLatencyMarker), which take any Direct3D 9 to 12
 * device: the Direct3D drivers' own, and the D3D12 device Vulkan's
 * DXGI presentation presents with. nvapi is resolved at runtime by its
 * published interface ids and never linked; a machine without it
 * reports the device reason.
 * One of these per driver instance, zeroed with it; the mode is a
 * property of @device, the device it was set on. The functions follow
 * the driver_pacing_* pokes (gfx/video_driver.h): set, wait and mark
 * are the frame loop's and touch only the mode half, which the
 * presenter's frame reads through @mode_pub; frame_begin and the
 * present pair are the presenter's and own the identity half. NVAPI
 * takes all of them from any thread. */
typedef struct d3d_driver_pacing
{
   uint64_t frame_id;
   void *device;
   retro_atomic_int_t mode_pub;  /* enum video_driver_pacing_mode on @device */
   uint32_t min_interval_us;
   uint8_t mode;
   uint8_t stage;     /* 1 rendering, 2 presented; later presents out of band */
   bool failed;
} d3d_driver_pacing_t;

unsigned d3d_driver_pacing_set(d3d_driver_pacing_t *s, void *device,
      unsigned mode);
bool d3d_driver_pacing_wait(d3d_driver_pacing_t *s, void *device,
      uint64_t frame_id, unsigned max_wait_us);
void d3d_driver_pacing_mark(d3d_driver_pacing_t *s, void *device,
      uint64_t frame_id, unsigned marker);
/* The driver's frame begins with the frame's identity and interval
 * from its info: once per identity, the simulation-end and render-
 * submit markers. Around every present: the frame's own gets its
 * markers, any other is out of band on the same identity. */
void d3d_driver_pacing_frame_begin(d3d_driver_pacing_t *s, void *device,
      uint64_t frame_id, unsigned min_interval_us);
void d3d_driver_pacing_present_begin(d3d_driver_pacing_t *s, void *device);
void d3d_driver_pacing_present_end(d3d_driver_pacing_t *s, void *device);
/* The presenter's view: whether the frame with @frame_id from its info
 * was released by the driver's sleep, the mode on and the frame named. */
bool d3d_driver_pacing_paced(d3d_driver_pacing_t *s, uint64_t frame_id);
/* The frame loop's side where the presenter is another thread and
 * owns @s: the sleep, and a marker, on the device the presenter last
 * put the mode on and published. False when the sleep failed. */
bool d3d_driver_pacing_sleep_on(void *device);
void d3d_driver_pacing_mark_on(void *device, uint64_t frame_id,
      unsigned marker);
/* The driver goes: the mode is taken off its device. */
void d3d_driver_pacing_free(d3d_driver_pacing_t *s, void *device);

RETRO_END_DECLS

#endif

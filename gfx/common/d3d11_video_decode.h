/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

#ifndef __D3D11_VIDEO_DECODE_H
#define __D3D11_VIDEO_DECODE_H

/* H.264 decoded on the GPU with D3D11 video (DXVA), as rh264's hardware
 * backend. Each decoder has a device of its own on the driver's
 * adapter, so the driver's device is never shared between threads; the
 * pictures to be shown are copied into shared NV12 textures, ordered
 * with shared fences, and imported by the driver - D3D11's or D3D12's
 * (video_poke_interface::import_texture). Windows 10 1703 or later. */

#include <retro_common_api.h>
#include <boolean.h>
#include <formats/rh265_hw.h>

RETRO_BEGIN_DECLS

/* Offers the backend to rh264 for decoders on @device's adapter (an
 * ID3D11Device). Main thread, once the device is up; false when the
 * system cannot share the frames. */
bool d3d11_video_decode_init(void *device);

/* The same for decoders on @adapter (an IDXGIAdapter), for a driver
 * that is not D3D11's: D3D12 opens the frames' handles as well. */
bool d3d11_video_decode_init_adapter(void *adapter);

/* Withdraws it and frees what closed decoders left. Every decoder must
 * be closed by then. */
void d3d11_video_decode_deinit(void);

/* Main 8-bit/NV12 only. The adapter must be initialized. Owner-thread
 * calls; close before deinit. A session borrows ops->decoder until close.
 * Output leases keep shared handles alive through the existing fences. */
bool d3d11_video_decode_hevc_open(const rh265_video_params_t *params,
      unsigned slots, rh265_hw_ops_t *ops);
void d3d11_video_decode_hevc_close(rh265_hw_ops_t *ops);

RETRO_END_DECLS

#endif

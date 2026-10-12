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
 * with shared fences, and imported by the driver
 * (video_poke_interface::import_texture). Windows 10 1703 or later. */

#include <retro_common_api.h>
#include <boolean.h>

RETRO_BEGIN_DECLS

/* Offers the backend to rh264 for decoders on @device's adapter (an
 * ID3D11Device). Main thread, once the device is up; false when the
 * system cannot share the frames. */
bool d3d11_video_decode_init(void *device);

/* Withdraws it and frees what closed decoders left. Every decoder must
 * be closed by then. */
void d3d11_video_decode_deinit(void);

RETRO_END_DECLS

#endif

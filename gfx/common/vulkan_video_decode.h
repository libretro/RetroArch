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

#ifndef __VULKAN_VIDEO_DECODE_H
#define __VULKAN_VIDEO_DECODE_H

/* H.264, HEVC Main/Main10, VP9 Profile 0/2 and AV1 Main on Vulkan Video.
 * The codec owner keeps sequencing; the GPU reconstructs
 * into a DPB. Unified video layouts allow direct sampled exports with
 * physical-slot renaming; other devices copy each visible picture into
 * an independent output image. The driver imports either export through
 * video_poke_interface::import_texture. Everything stays on the GPU.
 *
 * The device must have been made with the decode and transfer queues
 * (vulkan_context_t::video_lanes). */

#include <retro_common_api.h>
#include <boolean.h>

#include "vulkan_common.h"

RETRO_BEGIN_DECLS

/* Offers supported codecs to rh264/rh265/rvp9/rav1 for @ctx's device. Main thread, once the
 * device is up; false when the device has no video queues. */
bool vulkan_video_decode_init(const vulkan_context_t *ctx);

#if defined(_WIN32) && !defined(__WINRT__) \
   && (defined(HAVE_D3D11) || defined(HAVE_D3D12))
bool vulkan_video_decode_win32_fallback_init(const vulkan_context_t *ctx, unsigned formats);
#endif

/* Withdraws it and frees what closed decoders left, before the device
 * goes and once the driver has waited it idle. Every decoder must be
 * closed by then (the players are freed with the menu's context). */
void vulkan_video_decode_deinit(void);

RETRO_END_DECLS

#endif

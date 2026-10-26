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

#ifndef __D3D12_FFX_H
#define __D3D12_FFX_H

#include "d3d12_upscaler.h"

RETRO_BEGIN_DECLS

/* FSR on a Direct3D 12 device through the FidelityFX API
 * (libretro-common gfx/amd/ffx_loader.h), the Vulkan one's twin
 * (vulkan_ffx.h): one context per output size and flags, an output
 * texture the driver reads in the core's place. The library comes
 * from the FSR directory: SDK 2's loader runs FSR 4 on the GPUs that
 * have it and FSR 3.1 elsewhere; SDK 1.1's runs FSR 3.1. */
typedef struct d3d12_ffx d3d12_ffx_t;

d3d12_ffx_t *d3d12_ffx_init(ID3D12Device *device, const char *dir);

/* @mode is an FFX_UPSCALE_MODE_ */
bool d3d12_ffx_optimal(d3d12_ffx_t *ffx, unsigned mode,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims);

bool d3d12_ffx_upscale(d3d12_ffx_t *ffx, ID3D12GraphicsCommandList *cmd,
      const d3d12_upscaler_image_t *color,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned mode, unsigned out_dims, d3d12_upscaler_image_t *out);

void d3d12_ffx_frame_done(d3d12_ffx_t *ffx);
void d3d12_ffx_free(d3d12_ffx_t *ffx);

RETRO_END_DECLS

#endif

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

#ifndef __D3D12_XESS_H
#define __D3D12_XESS_H

#include "d3d12_upscaler.h"

RETRO_BEGIN_DECLS

/* XeSS Super Resolution on a Direct3D 12 device (libretro-common
 * gfx/intel/xess_loader.h), the Vulkan one's twin (vulkan_xess.h): a
 * context per output size, quality and flags, its pipelines built in
 * the library's background thread (the frames shown as they are until
 * they are ready), an output texture the driver reads in the core's
 * place. The library (libxess.dll) comes from the XeSS directory. */
typedef struct d3d12_xess d3d12_xess_t;

d3d12_xess_t *d3d12_xess_init(ID3D12Device *device, const char *dir);

/* @mode is an XESS_QUALITY_ */
bool d3d12_xess_optimal(d3d12_xess_t *xess, unsigned mode,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims);

bool d3d12_xess_upscale(d3d12_xess_t *xess, ID3D12GraphicsCommandList *cmd,
      const d3d12_upscaler_image_t *color,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned mode, unsigned out_dims, d3d12_upscaler_image_t *out);

void d3d12_xess_frame_done(d3d12_xess_t *xess);
void d3d12_xess_free(d3d12_xess_t *xess);

RETRO_END_DECLS

#endif

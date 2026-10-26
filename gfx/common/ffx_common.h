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

#ifndef _FFX_COMMON_H
#define _FFX_COMMON_H

#include <boolean.h>
#include <retro_common_api.h>
#include <libretro.h>
#include <gfx/amd/ffx_loader.h>

#include "../video_defines.h"

RETRO_BEGIN_DECLS

/* What the FSR backends share on either API: the context's creation
 * flags from a frame's inputs, the newest upscaler version the
 * library offers, the render sizes of a mode, the dispatch's scalars,
 * and the runtime's messages to the log. */

/* The FFX_UPSCALE_ENABLE_ flags a frame's inputs call for */
unsigned ffx_common_create_flags(const struct retro_hw_upscaler_inputs *inputs);

/* The version to ask for: the newest the library offers (@device for
 * Direct3D 12, NULL for Vulkan); 0 for the library's own default.
 * @name gets its name, or "" */
uint64_t ffx_common_version(const ffx_lib_t *lib, void *device,
      const char **name);

/* The mode's render size for @out_dims from the library, else by the
 * mode's ratio; the range it accepts: a third of the output up to the
 * output. @mode as video_upscaler_mode_ffx packs it. */
bool ffx_common_optimal(const ffx_lib_t *lib, unsigned mode,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims);

/* The dispatch's scalars: jitter, motion vector scale, sizes, exposure,
 * frame time, the camera, the sharpening (@sharpness percent, 0 off),
 * the sRGB flag of an SDR frame. The resources and command list are
 * the API's. */
void ffx_common_dispatch(ffx_upscale_dispatch_t *d,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned render_dims, unsigned out_dims, bool reset, unsigned sharpness);

/* The runtime's message callback: to the log */
void ffx_common_message(uint32_t type, const wchar_t *message);

const char *ffx_common_mode_name(unsigned mode);

RETRO_END_DECLS

#endif

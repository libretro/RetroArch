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

#ifndef __VIDEO_UPSCALER_H
#define __VIDEO_UPSCALER_H

#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>
#include <libretro.h>

#include "video_defines.h"

RETRO_BEGIN_DECLS

/* The temporal upscaler (video_temporal_upscaler): a backend the Vulkan
 * and Direct3D 12 drivers run over the color, depth, motion vectors and
 * jitter a hardware-rendered core exports. Each runs on its vendor's
 * library, loaded at run time from a directory of the user's (nothing
 * of theirs is built in or shipped; the application directory's
 * upscalers/<name> when the setting is empty):
 *  - DLSS Super Resolution on NVIDIA's NGX (libretro-common
 *    gfx/nvidia): the driver's NGX core library, and the DLSS feature
 *    library in the DLSS directory;
 *  - FSR, AMD's FidelityFX Super Resolution through the FidelityFX API
 *    (gfx/amd): its library in the FSR directory, FSR 3.1 on either
 *    driver, FSR 4 on Direct3D 12 where SDK 2's library offers it;
 *  - XeSS, Intel's Super Resolution (gfx/intel): libxess in the XeSS
 *    directory, on either driver. */
enum video_temporal_upscaler
{
   VIDEO_TEMPORAL_UPSCALER_OFF = 0,
   VIDEO_TEMPORAL_UPSCALER_DLSS,
   VIDEO_TEMPORAL_UPSCALER_FSR,
   VIDEO_TEMPORAL_UPSCALER_XESS,
   VIDEO_TEMPORAL_UPSCALER_LAST
};

/* What a probe found, as video_upscaler_probe reports it */
#define VIDEO_UPSCALER_FOUND_LIBRARY (1 << 0)   /* the API's library: NGX's core, FSR's, XeSS's */
#define VIDEO_UPSCALER_FOUND_RUNTIME (1 << 1)   /* the feature library in the directory (DLSS); the library itself (FSR, XeSS) */
#define VIDEO_UPSCALER_READY \
   (VIDEO_UPSCALER_FOUND_LIBRARY | VIDEO_UPSCALER_FOUND_RUNTIME)

/* What the probe of @kind found, to the log: the libraries, where they
 * were looked for */
void video_upscaler_log_probe(enum video_temporal_upscaler kind);

/* The directory of @kind in effect into @s: the setting, else the
 * application directory's upscalers/<dlss|fsr|xess>. Empty for Off. */
size_t video_upscaler_dir(enum video_temporal_upscaler kind, char *s, size_t len);

/* The VIDEO_UPSCALER_FOUND_* bits of @kind found now. A library found
 * stays loaded. */
unsigned video_upscaler_probe(enum video_temporal_upscaler kind);

/* The setting and what the probe found, for System Information; fits
 * NAME_MAX_LENGTH */
size_t video_upscaler_status_string(char *s, size_t len);

/* The upscaler's name for a log or the status */
const char *video_upscaler_name(enum video_temporal_upscaler kind);

/* video_temporal_upscaler_quality: the mode, as the backend names its
 * render size for it; Auto picks by output size. */
enum video_temporal_upscaler_quality
{
   VIDEO_TEMPORAL_UPSCALER_QUALITY_AUTO = 0,
   VIDEO_TEMPORAL_UPSCALER_QUALITY_NATIVE,       /* DLAA: render at output size */
   VIDEO_TEMPORAL_UPSCALER_QUALITY_QUALITY,
   VIDEO_TEMPORAL_UPSCALER_QUALITY_BALANCED,
   VIDEO_TEMPORAL_UPSCALER_QUALITY_PERFORMANCE,
   VIDEO_TEMPORAL_UPSCALER_QUALITY_ULTRA_PERFORMANCE,
   VIDEO_TEMPORAL_UPSCALER_QUALITY_LAST
};

/* video_temporal_upscaler_preset: the DLSS model (ngx_dlss_render_preset) */
enum video_temporal_upscaler_preset
{
   VIDEO_TEMPORAL_UPSCALER_PRESET_DEFAULT = 0,
   VIDEO_TEMPORAL_UPSCALER_PRESET_J,
   VIDEO_TEMPORAL_UPSCALER_PRESET_K,
   VIDEO_TEMPORAL_UPSCALER_PRESET_LAST
};

/* The mode for an output of @out_dims (VIDEO_SCALE_PACK): the quality
 * setting, Auto resolved, and the preset, as one word
 * (VIDEO_UPSCALER_MODE_PACK), the backends' own mode words by the
 * translations below. */
#define VIDEO_UPSCALER_MODE_PACK(quality, preset) ((unsigned)(quality) | ((unsigned)(preset) << 8))
#define VIDEO_UPSCALER_MODE_QUALITY(mode)         ((mode) & 0xffu)   /* enum video_temporal_upscaler_quality, never Auto */
#define VIDEO_UPSCALER_MODE_PRESET(mode)          ((mode) >> 8)      /* enum video_temporal_upscaler_preset */
unsigned video_upscaler_mode(unsigned out_dims);

/* NGX_MODE_PACK: an enum ngx_perf_quality and an enum
 * ngx_dlss_render_preset */
unsigned video_upscaler_mode_ngx(unsigned mode);
/* An FFX_UPSCALE_MODE_ (the preset has no FSR meaning) */
unsigned video_upscaler_mode_ffx(unsigned mode);
/* An XESS_QUALITY_ */
unsigned video_upscaler_mode_xess(unsigned mode);

/* A video driver's upscaler, attached while its session is up:
 * optimal names the mode's render sizes for an output size, false
 * when the session cannot; sizes are VIDEO_SCALE_PACK. @data is the
 * driver's. */
typedef struct video_upscaler_backend
{
   void *data;
   bool (*optimal)(void *data, unsigned mode, unsigned out_dims,
         unsigned *render_dims, unsigned *min_dims, unsigned *max_dims);
} video_upscaler_backend_t;

void video_upscaler_attach(const video_upscaler_backend_t *backend);
void video_upscaler_detach(void);

/* The content's size on screen (VIDEO_SCALE_PACK), the output the core
 * is asked to aim for; the driver reports it at init and whenever it
 * changes. */
void video_upscaler_set_output(unsigned dims);

/* The interface handed to the core (RETRO_ENVIRONMENT_GET_HW_RENDER_
 * UPSCALER_INTERFACE); NULL without HAVE_NGX or a driver that can. */
const struct retro_hw_render_upscaler_interface *video_upscaler_get_interface(void);

/* The inputs the core set for the frame being presented, taken: NULL
 * when it set none. The driver reads them inside video_refresh. */
const struct retro_hw_upscaler_inputs *video_upscaler_take_inputs(void);

RETRO_END_DECLS

#endif

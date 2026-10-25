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
 * jitter a hardware-rendered core exports. DLSS Super Resolution runs
 * on NVIDIA's NGX, loaded at run time (libretro-common gfx/nvidia):
 * the driver's NGX core library, and the DLSS feature library the user
 * puts in the DLSS directory (video_dlss_directory; the application
 * directory's upscalers/dlss when empty). Nothing NVIDIA is built in or
 * shipped. */
enum video_temporal_upscaler
{
   VIDEO_TEMPORAL_UPSCALER_OFF = 0,
   VIDEO_TEMPORAL_UPSCALER_DLSS,
   VIDEO_TEMPORAL_UPSCALER_LAST
};

/* What a probe found, as video_upscaler_dlss_probe reports it */
#define VIDEO_UPSCALER_DLSS_NGX     (1 << 0)   /* the NGX core library */
#define VIDEO_UPSCALER_DLSS_FEATURE (1 << 1)   /* the feature library in the directory */
#define VIDEO_UPSCALER_DLSS_READY \
   (VIDEO_UPSCALER_DLSS_NGX | VIDEO_UPSCALER_DLSS_FEATURE)

/* What the probe found, to the log: the NGX core library, where it
 * was looked for, and the DLSS runtime in the DLSS directory */
void video_upscaler_log_probe(void);

/* The DLSS directory in effect into @s: the setting, else the application directory's upscalers/dlss */
size_t video_upscaler_dlss_dir(char *s, size_t len);

/* The VIDEO_UPSCALER_DLSS_* bits found now: the menu offers DLSS when
 * both are there. A library found stays loaded. */
unsigned video_upscaler_dlss_probe(void);

/* The setting and what the probe found, for System Information; fits
 * NAME_MAX_LENGTH */
size_t video_upscaler_status_string(char *s, size_t len);

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
 * setting, Auto resolved, and the preset, as one word (NGX_MODE_PACK:
 * an enum ngx_perf_quality and an enum ngx_dlss_render_preset). */
unsigned video_upscaler_mode(unsigned out_dims);

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

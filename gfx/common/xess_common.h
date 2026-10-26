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

#ifndef _XESS_COMMON_H
#define _XESS_COMMON_H

#include <boolean.h>
#include <retro_common_api.h>
#include <libretro.h>
#include <gfx/intel/xess_loader.h>

#include "../video_defines.h"

RETRO_BEGIN_DECLS

/* What the XeSS backends share on either API: the context's life from
 * creation through its pipelines building to init, the init flags a
 * frame's inputs call for, the render sizes of a quality, the scales
 * set on a context, and what failed last. */
typedef struct xess_common_state
{
   /* A context of the session's own, never initialised, for the size
    * queries before a frame brings one */
   xess_context_t query;
   /* What the frame's context is for; sizes are VIDEO_SCALE_PACK */
   unsigned out_dims;
   unsigned mode;             /* XESS_QUALITY_ */
   unsigned flags;            /* XESS_INIT_FLAG_ */
   /* What failed last: not tried again until these change */
   unsigned failed_out_dims;
   unsigned failed_mode;
   unsigned failed_flags;
   float velocity_x;
   float velocity_y;
   bool inited;               /* init done: execute may run */
   bool reset_pending;        /* the first execute after init starts the history */
   bool failed_valid;
} xess_common_state_t;

/* The XESS_INIT_FLAG_ bits a frame's inputs call for */
unsigned xess_common_init_flags(const struct retro_hw_upscaler_inputs *inputs);

/* The quality's render size for @out_dims from the library's query
 * context, else by the quality's ratio; the range it accepts */
bool xess_common_optimal(const xess_lib_t *lib, xess_common_state_t *st,
      unsigned mode, unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims);

/* A context made for @mode, @flags and @out_dims: not inited yet */
void xess_common_created(const xess_lib_t *lib, xess_common_state_t *st,
      xess_context_t ctx, unsigned mode, unsigned flags, unsigned out_dims);
/* Its init done: the jitter scale set, the history to start over */
void xess_common_inited(const xess_lib_t *lib, xess_common_state_t *st,
      xess_context_t ctx);
/* The frame's motion vector scale onto the context, when it changed */
void xess_common_scales(const xess_lib_t *lib, xess_common_state_t *st,
      xess_context_t ctx, const struct retro_hw_upscaler_inputs *inputs);

void xess_common_failed(xess_common_state_t *st, unsigned out_dims,
      unsigned mode, unsigned flags);
bool xess_common_has_failed(const xess_common_state_t *st, unsigned out_dims,
      unsigned mode, unsigned flags);

/* The library's log callback: to the log */
void xess_common_log(const char *message, int level);

const char *xess_common_mode_name(unsigned mode);

RETRO_END_DECLS

#endif

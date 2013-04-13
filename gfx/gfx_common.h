/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#ifndef __GFX_COMMON_H
#define __GFX_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "../general.h"
#include "../boolean.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

// If always_write is true, will always update FPS value
// If always_write is false, returns true if FPS value was updated.
bool gfx_get_fps(char *buf, size_t size, bool always_write);

#ifdef _WIN32
void gfx_set_dwm(void);
#endif

void gfx_scale_integer(struct rarch_viewport *vp, unsigned win_width, unsigned win_height,
      float aspect_ratio, bool keep_aspect);

typedef struct
{
   float x;
   float y;
   float scale;
   unsigned color;
} font_params_t;

#define MIN_SCALING_FACTOR (1.0f)

#if defined(__CELLOS_LV2__)
#define MAX_SCALING_FACTOR (5.0f)
#else
#define MAX_SCALING_FACTOR (2.0f)
#endif

enum aspect_ratio
{
   ASPECT_RATIO_1_1 = 0,
   ASPECT_RATIO_2_1,
   ASPECT_RATIO_3_2,
   ASPECT_RATIO_3_4,
   ASPECT_RATIO_4_1,
   ASPECT_RATIO_4_3,
   ASPECT_RATIO_4_4,
   ASPECT_RATIO_5_4,
   ASPECT_RATIO_6_5,
   ASPECT_RATIO_7_9,
   ASPECT_RATIO_8_3,
   ASPECT_RATIO_8_7,
   ASPECT_RATIO_16_9,
   ASPECT_RATIO_16_10,
   ASPECT_RATIO_16_15,
   ASPECT_RATIO_19_12,
   ASPECT_RATIO_19_14,
   ASPECT_RATIO_30_17,
   ASPECT_RATIO_32_9,
   ASPECT_RATIO_AUTO,
   ASPECT_RATIO_CORE,
   ASPECT_RATIO_CUSTOM,

   ASPECT_RATIO_END,
};

#define LAST_ASPECT_RATIO ASPECT_RATIO_CUSTOM

enum rotation
{
   ORIENTATION_NORMAL = 0,
   ORIENTATION_VERTICAL,
   ORIENTATION_FLIPPED,
   ORIENTATION_FLIPPED_ROTATED,
   ORIENTATION_END
};

#define LAST_ORIENTATION (ORIENTATION_END - 1)

extern char rotation_lut[ASPECT_RATIO_END][32];

/* ABGR color format defines */

#define WHITE		0xffffffffu
#define RED         0xff0000ffu
#define GREEN		0xff00ff00u
#define BLUE		0xffff0000u
#define YELLOW		0xff00ffffu
#define PURPLE		0xffff00ffu
#define CYAN		0xffffff00u
#define ORANGE		0xff0063ffu
#define SILVER		0xff8c848cu
#define LIGHTBLUE	0xFFFFE0E0U
#define LIGHTORANGE	0xFFE0EEFFu

struct aspect_ratio_elem
{
   char name[64];
   float value;
};

extern struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END];

extern void gfx_set_auto_viewport(unsigned width, unsigned height);
extern void gfx_set_core_viewport(void);

#ifdef __cplusplus
}
#endif

#endif

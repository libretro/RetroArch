/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#ifndef RARCH_CONSOLE_VIDEO_H__
#define RARCH_CONSOLE_VIDEO_H__

#define IS_TIMER_NOT_EXPIRED(index)        (g_extern.frame_count < g_extern.console.general_timers[(index)].expire_frame)
#define IS_TIMER_EXPIRED(index)            (!(IS_TIMER_NOT_EXPIRED(index)))
#define SET_TIMER_EXPIRATION(index, value) (g_extern.console.general_timers[(index)].expire_frame = g_extern.frame_count + (value))

#define MIN_SCALING_FACTOR (1.0f)

#if defined(__CELLOS_LV2__)
#define MAX_SCALING_FACTOR (5.0f)
#else
#define MAX_SCALING_FACTOR (2.0f)
#endif

enum
{
   FBO_DEINIT = 0,
   FBO_INIT,
   FBO_REINIT
};

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

#define LAST_ORIENTATION (ORIENTATION_END-1)

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

extern void rarch_set_auto_viewport(unsigned width, unsigned height);
extern void rarch_set_core_viewport(void);

#endif

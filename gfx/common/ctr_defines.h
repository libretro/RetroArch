/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouahl
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

#ifndef CTR_DEFINES_H__
#define CTR_DEFINES_H__

#include <3ds.h>
#include <retro_inline.h>

#define COLOR_ABGR(r, g, b, a) (((unsigned)(a) << 24) | ((b) << 16) | ((g) << 8) | ((r) << 0))

#define CTR_TOP_FRAMEBUFFER_WIDTH      400
#define CTR_TOP_FRAMEBUFFER_HEIGHT     240
#define CTR_BOTTOM_FRAMEBUFFER_WIDTH   320
#define CTR_BOTTOM_FRAMEBUFFER_HEIGHT  240
#define CTR_STATE_DATE_SIZE            11

typedef enum
{
   CTR_VIDEO_MODE_3D = 0,
   CTR_VIDEO_MODE_2D,
   CTR_VIDEO_MODE_2D_400X240,
   CTR_VIDEO_MODE_2D_800X240,
   CTR_VIDEO_MODE_LAST
} ctr_video_mode_enum;

#ifdef USE_CTRULIB_2
extern u8* gfxTopLeftFramebuffers[2];
extern u8* gfxTopRightFramebuffers[2];
extern u8* gfxBottomFramebuffers[2];
#endif

#ifdef CONSOLE_LOG
extern PrintConsole* ctrConsole;
#endif

extern const u8 ctr_sprite_shbin[];
extern const u32 ctr_sprite_shbin_size;

#endif /* CTR_DEFINES_H__ */

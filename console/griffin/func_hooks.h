/*  SSNES - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _FUNC_HOOKS_H
#define _FUNC_HOOKS_H

/*============================================================
	GENERAL
============================================================ */
#if defined(__CELLOS_LV2__) || defined(_XBOX)
#define HAVE_GRIFFIN_OVERRIDE_VIDEO_FRAME_FUNC 1
#endif

#define ssnes_render_cached_frame() \
   const char *msg = msg_queue_pull(g_extern.msg_queue); \
   video_frame_func(g_extern.frame_cache.data, g_extern.frame_cache.width, g_extern.frame_cache.height, g_extern.frame_cache.height, msg);

#endif

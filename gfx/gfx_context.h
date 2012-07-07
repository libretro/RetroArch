/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#ifndef __GFX_CONTEXT_H
#define __GFX_CONTEXT_H

#include "../boolean.h"
#include "../driver.h"


#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_OPENGL
#include "gl_common.h"
#define VID_HANDLE gl_t
#endif

#ifdef HAVE_D3D9
#define VID_HANDLE xdk_d3d_video_t
#endif

#ifdef HAVE_SDL
#include "SDL_syswm.h"
#endif

void gfx_ctx_set_swap_interval(unsigned interval, bool inited);

bool gfx_ctx_set_video_mode(
      unsigned width, unsigned height,
      unsigned bits, bool fullscreen);

bool gfx_ctx_init(void);
void gfx_ctx_destroy(void);

void gfx_ctx_get_video_size(unsigned *width, unsigned *height);
void gfx_ctx_update_window_title(bool reset);

bool gfx_ctx_key_pressed(int key);

void gfx_ctx_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count);

void gfx_ctx_set_resize(unsigned width, unsigned height);

#ifdef HAVE_SDL
bool gfx_ctx_get_wm_info(SDL_SysWMinfo *info);
#endif

#ifndef HAVE_GRIFFIN
bool gfx_ctx_window_has_focus(void);

void gfx_ctx_swap_buffers(void);
#endif

void gfx_ctx_input_driver(const input_driver_t **input, void **input_data);

#ifdef HAVE_CG_MENU
bool gfx_ctx_menu_init(void);
#endif

#ifdef RARCH_CONSOLE
void gfx_ctx_set_filtering(unsigned index, bool set_smooth);
void gfx_ctx_get_available_resolutions(void);
int gfx_ctx_check_resolution(unsigned resolution_id);
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_D3D9) || defined(HAVE_D3D8)
void gfx_ctx_set_projection(VID_HANDLE *gl, const struct gl_ortho *ortho, bool allow_rotate);
#endif

#endif


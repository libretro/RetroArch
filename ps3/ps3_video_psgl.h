/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 *
 *  Some code herein may be based on code found in BSNES.
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

#ifndef _PS3_VIDEO_PSGL_H
#define _PS3_VIDEO_PSGL_H

#include "../gfx/gl_common.h"
#include "../gfx/gfx_common.h"
#include "../gfx/image.h"
#include "menu-port-defines.h"
#include <cell/dbgfont.h>

#define FBO_DEINIT	0
#define FBO_INIT	1
#define FBO_REINIT	2

#define MAX_SHADERS 16

#define TEXTURES 8
#define TEXTURES_MASK (TEXTURES - 1)

#define MIN_SCALING_FACTOR (1.0f)
#define MAX_SCALING_FACTOR (4.0f)

#define IS_TIMER_NOT_EXPIRED(getter) (g_frame_count < getter)
#define IS_TIMER_EXPIRED(getter) 	(!(IS_TIMER_NOT_EXPIRED(getter)))
#define SET_TIMER_EXPIRATION(setter, value) setter = g_frame_count + value;

enum
{
	ASPECT_RATIO_4_3,
	ASPECT_RATIO_5_4,
	ASPECT_RATIO_8_7,
	ASPECT_RATIO_16_9,
	ASPECT_RATIO_16_10,
	ASPECT_RATIO_16_15,
	ASPECT_RATIO_19_14,
	ASPECT_RATIO_2_1,
	ASPECT_RATIO_3_2,
	ASPECT_RATIO_3_4,
	ASPECT_RATIO_1_1,
	ASPECT_RATIO_AUTO,
	ASPECT_RATIO_CUSTOM,
	LAST_ASPECT_RATIO
};

typedef struct gl
{
   bool block_swap;
   bool fbo_inited;
   bool keep_aspect;
   bool render_to_tex;
   bool should_resize;
   bool vsync;
   bool overscan_enable;
   int fbo_pass;
   unsigned base_size; /* 2 or 4*/
   unsigned last_width[TEXTURES];
   unsigned last_height[TEXTURES];
   unsigned tex_index; /* For use with PREV. */
   unsigned tex_w, tex_h;
   unsigned vp_width, vp_out_width;
   unsigned vp_height, vp_out_height;
   unsigned win_width;
   unsigned win_height;
   GLfloat overscan_amount;
   GLfloat tex_coords[8];
   GLfloat fbo_tex_coords[8];
   GLenum texture_type; /* XBGR1555 or ARGB*/
   GLenum texture_fmt;
   /* Render-to-texture, multipass shaders */
   GLuint fbo[MAX_SHADERS];
   GLuint fbo_texture[MAX_SHADERS];
   GLuint menu_texture_id;
   GLuint pbo;
   GLuint texture[TEXTURES];
   GLuint tex_filter;
   CellVideoOutState g_video_state;
   PSGLdevice* gl_device;
   PSGLcontext* gl_context;
   struct gl_fbo_rect fbo_rect[MAX_SHADERS];
   struct gl_fbo_scale fbo_scale[MAX_SHADERS];
   struct gl_tex_info prev_info[TEXTURES];
   struct texture_image menu_texture;
   void *empty_buf;
} gl_t;


bool ps3_setup_texture(void);
const char * ps3_get_resolution_label(uint32_t resolution);
int ps3_check_resolution(uint32_t resolution_id);
void gl_frame_menu(void);
void gl_deinit_fbo(gl_t * gl);
void gl_init_fbo(gl_t * gl, unsigned width, unsigned height);
void ps3_previous_resolution (void);
void ps3_next_resolution (void);
void ps3_set_filtering(unsigned index, bool set_smooth);
void ps3_video_deinit(void);
void ps3graphics_reinit_fbos (void);
void ps3graphics_set_aspect_ratio(uint32_t aspectratio_index);
void ps3graphics_set_overscan(bool overscan_enable, float amount, bool recalculate_viewport);
void ps3graphics_set_orientation(uint32_t orientation);
void ps3graphics_set_vsync(uint32_t vsync);
void ps3graphics_video_init(bool get_all_resolutions);
void ps3graphics_video_reinit(void);

extern void *g_gl;
extern unsigned g_frame_count;

#endif

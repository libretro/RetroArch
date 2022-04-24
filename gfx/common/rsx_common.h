/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef RSX_COMMON_H__
#define RSX_COMMON_H__

#include <rsx/rsx.h>
#include <rsx/nv40.h>
#include <ppu-types.h>

#include <retro_inline.h>
#include <string/stdstring.h>
#include <gfx/math/matrix_4x4.h>

#include <defines/ps3_defines.h>
#include "../../driver.h"
#include "../../retroarch.h"
#include "../video_coord_array.h"

#define MAX_BUFFERS 2
#define MAX_MENU_BUFFERS 2

/* Shader objects */
extern const u8 vpshader_basic_vpo_end[];
extern const u8 vpshader_basic_vpo[];
extern const u32 vpshader_basic_vpo_size;

extern const u8 fpshader_basic_fpo_end[];
extern const u8 fpshader_basic_fpo[];
extern const u32 fpshader_basic_fpo_size;

typedef struct
{
   float x, y;
   float w, h;
   float min, max;
   float scale[4];
   float offset[4];
} rsx_viewport_t;

typedef struct
{
   float x, y, z;
   float nx, ny, nz;
   float u, v;
   float r, g, b, a;
} rsx_vertex_t;

typedef struct
{
   gcmTexture tex;
   u32 *data;
   u32 offset;
   u32 wrap_s;
   u32 wrap_t;
   u32 min_filter;
   u32 mag_filter;
   u32 width;
   u32 height;
} rsx_texture_t;

typedef struct
{
   int height;
   int width;
   int id;
   uint32_t *ptr;
   /* Internal stuff */
   uint32_t offset;
} rsxBuffer;

typedef struct {
   video_viewport_t vp;
   rsxBuffer buffers[MAX_BUFFERS];
#if defined(HAVE_MENU_BUFFER)
   rsxBuffer menuBuffers[MAX_MENU_BUFFERS];
   int menuBuffer;
#endif
   int currentBuffer, nextBuffer;
   gcmContextData* context;
   u16 width;
   u16 height;
   u16 menu_width;
   u16 menu_height;
   bool menu_frame_enable;
   bool rgb32;
   bool vsync;
   u32 depth_pitch;
   u32 depth_offset;
   u32* depth_buffer;

#if defined(HAVE_MENU_BUFFER)
   gcmSurface surface[MAX_BUFFERS+MAX_MENU_BUFFERS];
#else
   gcmSurface surface[MAX_BUFFERS];
#endif
   rsx_texture_t texture;
   rsx_texture_t menu_texture;
   rsx_vertex_t *vertices;
   u32 pos_offset;
   u32 uv_offset;
   u32 col_offset;
   rsxProgramConst  *proj_matrix;
   rsxProgramAttrib* pos_index;
   rsxProgramAttrib* col_index;
   rsxProgramAttrib* uv_index;
   rsxProgramAttrib* tex_unit;
   void *vp_ucode;
   void *fp_ucode;
   rsxVertexProgram *vpo;
   rsxFragmentProgram *fpo;
   u32 *fp_buffer;
   u32 fp_offset;
   math_matrix_4x4 mvp, mvp_no_rot;
   float menu_texture_alpha;

   bool smooth;
   unsigned rotation;
   bool keep_aspect;
   bool should_resize;
   bool msg_rendering_enabled;

   const shader_backend_t* shader;
   void* shader_data;
   void* renderchain_data;
   void* ctx_data;
   const gfx_ctx_driver_t* ctx_driver;
   bool shared_context_use;

   video_info_t video_info;
   struct video_tex_info tex_info;                    /* unsigned int alignment */
   struct video_tex_info prev_info[GFX_MAX_TEXTURES]; /* unsigned alignment */
   struct video_fbo_rect fbo_rect[GFX_MAX_SHADERS];   /* unsigned alignment */
} rsx_t;

#endif

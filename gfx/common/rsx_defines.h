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

#ifndef RSX_DEFINES_H__
#define RSX_DEFINES_H__

#include <retro_inline.h>
#include <string/stdstring.h>
#include <gfx/math/matrix_4x4.h>

#include <defines/ps3_defines.h>

#ifdef __PSL1GHT__
#include <rsx/rsx.h>
#include <rsx/nv40.h>
#include <ppu-types.h>
#endif

#include "../../driver.h"
#include "../../retroarch.h"

#define RSX_MAX_BUFFERS 2
#define RSX_MAX_MENU_BUFFERS 2
#define RSX_MAX_TEXTURES 4
#define RSX_MAX_SHADERS 2
#define RSX_MAX_VERTICES 4
#define RSX_MAX_TEXTURE_VERTICES 4096 /* Set > 0 for preallocated texture vertices */
#define RSX_MAX_FONT_VERTICES 8192

#define RSX_SHADER_STOCK_BLEND (RSX_MAX_SHADERS - 1)
#define RSX_SHADER_MENU        (RSX_MAX_SHADERS - 2)

/* Shader objects */
extern const u8 modern_opaque_vpo_end[];
extern const u8 modern_opaque_vpo[];
extern const u32 modern_opaque_vpo_size;
extern const u8 modern_opaque_fpo_end[];
extern const u8 modern_opaque_fpo[];
extern const u32 modern_opaque_fpo_size;

extern const u8 modern_alpha_blend_vpo_end[];
extern const u8 modern_alpha_blend_vpo[];
extern const u32 modern_alpha_blend_vpo_size;
extern const u8 modern_alpha_blend_fpo_end[];
extern const u8 modern_alpha_blend_fpo[];
extern const u32 modern_alpha_blend_fpo_size;

typedef struct
{
   float x, y;
   float w, h;
   float min, max;
   float scale[4];
   float offset[4];
} rsx_viewport_t;

typedef struct __attribute__((aligned(128)))
{
   float x, y;
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

#ifdef HAVE_OVERLAY
typedef struct
{
   rsx_vertex_t *vertices;
   rsx_texture_t texture;
} rsx_overlay_t;
#endif

typedef struct
{
   int height;
   int width;
   int id;
   uint32_t *ptr;
   /* Internal stuff */
   uint32_t offset;
} rsxBuffer;

typedef struct
{
   video_viewport_t vp;
   rsxBuffer buffers[RSX_MAX_BUFFERS];
#if defined(HAVE_MENU_BUFFER)
   rsxBuffer menuBuffers[RSX_MAX_MENU_BUFFERS];
   int menuBuffer;
#endif
   int currentBuffer, nextBuffer;
   gcmContextData* context;
   u32* depth_buffer;

#if defined(HAVE_MENU_BUFFER)
   gcmSurface surface[RSX_MAX_BUFFERS + RSX_MAX_MENU_BUFFERS];
#else
   gcmSurface surface[RSX_MAX_BUFFERS];
#endif
   rsx_texture_t texture[RSX_MAX_TEXTURES];
   rsx_texture_t menu_texture;
   rsx_vertex_t *vertices;
   rsx_vertex_t *texture_vertices;
   int tex_index;
   int vert_idx;
   int texture_vert_idx;
   int font_vert_idx;
   u32 pos_offset[RSX_MAX_SHADERS];
   u32 uv_offset[RSX_MAX_SHADERS];
   u32 col_offset[RSX_MAX_SHADERS];
   rsxProgramConst  *proj_matrix[RSX_MAX_SHADERS];
   rsxProgramConst  *bgcolor[RSX_MAX_SHADERS];
   rsxProgramAttrib *pos_index[RSX_MAX_SHADERS];
   rsxProgramAttrib *col_index[RSX_MAX_SHADERS];
   rsxProgramAttrib *uv_index[RSX_MAX_SHADERS];
   rsxProgramAttrib *tex_unit[RSX_MAX_SHADERS];
   void *vp_ucode[RSX_MAX_SHADERS];
   void *fp_ucode[RSX_MAX_SHADERS];
   rsxVertexProgram *vpo[RSX_MAX_SHADERS];
   rsxFragmentProgram *fpo[RSX_MAX_SHADERS];
   u32 *fp_buffer[RSX_MAX_SHADERS];
   u32 fp_offset[RSX_MAX_SHADERS];
   math_matrix_4x4 mvp, mvp_no_rot;
   const shader_backend_t* shader;
   void* shader_data;
   void* renderchain_data;
   void* ctx_data;
   const gfx_ctx_driver_t* ctx_driver;

   video_info_t video_info;

   float menu_texture_alpha;

#ifdef HAVE_OVERLAY
   rsx_overlay_t *overlay;
   unsigned overlays;
   bool overlay_enable;
   bool overlay_full_screen;
#endif

   unsigned rotation;

   u16 width;
   u16 height;
   u16 menu_width;
   u16 menu_height;
   u32 depth_pitch;
   u32 depth_offset;

   bool menu_frame_enable;
   bool rgb32;
   bool vsync;
   bool smooth;
   bool keep_aspect;
   bool should_resize;
   bool msg_rendering_enabled;
   bool shared_context_use;
} rsx_t;

#endif

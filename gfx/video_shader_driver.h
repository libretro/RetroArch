/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifndef VIDEO_SHADER_DRIVER_H__
#define VIDEO_SHADER_DRIVER_H__

#include <boolean.h>

#include <gfx/math/matrix_4x4.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
#ifndef HAVE_SHADER_MANAGER
#define HAVE_SHADER_MANAGER
#endif

#include "video_shader_parse.h"

#define GL_SHADER_STOCK_BLEND (GFX_MAX_SHADERS - 1)

#endif

#if defined(_XBOX360)
#define DEFAULT_SHADER_TYPE RARCH_SHADER_HLSL
#elif defined(__PSL1GHT__)
#define DEFAULT_SHADER_TYPE RARCH_SHADER_GLSL
#elif defined(__CELLOS_LV2__)
#define DEFAULT_SHADER_TYPE RARCH_SHADER_CG
#elif defined(HAVE_OPENGLES2)
#define DEFAULT_SHADER_TYPE RARCH_SHADER_GLSL
#else
#define DEFAULT_SHADER_TYPE RARCH_SHADER_NONE
#endif

#include "video_context_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

enum video_shader_driver_ctl_state
{
   SHADER_CTL_NONE = 0,
   SHADER_CTL_DEINIT,
   SHADER_CTL_INIT,
   /* Finds first suitable shader context driver. */
   SHADER_CTL_INIT_FIRST,
   SHADER_CTL_SET_PARAMS,
   SHADER_CTL_GET_FEEDBACK_PASS,
   SHADER_CTL_MIPMAP_INPUT,
   SHADER_CTL_SET_COORDS,
   SHADER_CTL_SCALE,
   SHADER_CTL_INFO,
   SHADER_CTL_SET_MVP,
   SHADER_CTL_FILTER_TYPE,
   SHADER_CTL_USE,
   SHADER_CTL_WRAP_TYPE,
   SHADER_CTL_GET_CURRENT_SHADER,
   SHADER_CTL_DIRECT_GET_CURRENT_SHADER,
   SHADER_CTL_GET_IDENT
};

typedef struct shader_backend
{
   void *(*init)(void *data, const char *path);
   void (*deinit)(void *data);
   void (*set_params)(void *data, void *shader_data,
         unsigned width, unsigned height, 
         unsigned tex_width, unsigned tex_height, 
         unsigned out_width, unsigned out_height,
         unsigned frame_counter,
         const void *info, 
         const void *prev_info,
         const void *feedback_info,
         const void *fbo_info, unsigned fbo_info_cnt);

   void (*use)(void *data, void *shader_data, unsigned index);
   unsigned (*num_shaders)(void *data);
   bool (*filter_type)(void *data, unsigned index, bool *smooth);
   enum gfx_wrap_type (*wrap_type)(void *data, unsigned index);
   void (*shader_scale)(void *data,
         unsigned index, struct gfx_fbo_scale *scale);
   bool (*set_coords)(void *handle_data,
         void *shader_data, const void *data);
   bool (*set_mvp)(void *data, void *shader_data,
         const math_matrix_4x4 *mat);
   unsigned (*get_prev_textures)(void *data);
   bool (*get_feedback_pass)(void *data, unsigned *pass);
   bool (*mipmap_input)(void *data, unsigned index);

   struct video_shader *(*get_current_shader)(void *data);

   enum rarch_shader_type type;

   /* Human readable string. */
   const char *ident;
} shader_backend_t;

typedef struct video_shader_ctx_init
{
   const shader_backend_t *shader;
   void *data;
   const char *path;
} video_shader_ctx_init_t;

typedef struct video_shader_ctx_params
{
   void *data;
   unsigned width;
   unsigned height;
   unsigned tex_width;
   unsigned tex_height;
   unsigned out_width;
   unsigned out_height;
   unsigned frame_counter;
   const void *info;
   const void *prev_info;
   const void *feedback_info;
   const void *fbo_info;
   unsigned fbo_info_cnt;
} video_shader_ctx_params_t;

typedef struct video_shader_ctx_coords
{
   void *handle_data;
   const void *data;
} video_shader_ctx_coords_t;

typedef struct video_shader_ctx_scale
{
   unsigned idx;
   struct gfx_fbo_scale *scale;
} video_shader_ctx_scale_t;

typedef struct video_shader_ctx_info
{
   unsigned num;
   unsigned idx;
   void *data;
} video_shader_ctx_info_t;

typedef struct video_shader_ctx_mvp
{
   void *data;
   const math_matrix_4x4 *matrix;
} video_shader_ctx_mvp_t;

typedef struct video_shader_ctx_filter
{
   unsigned index;
   bool *smooth;
} video_shader_ctx_filter_t;

typedef struct video_shader_ctx_wrap
{
   unsigned idx;
   enum gfx_wrap_type type;
} video_shader_ctx_wrap_t;

typedef struct video_shader_ctx
{
   struct video_shader *data;
} video_shader_ctx_t;

typedef struct video_shader_ctx_ident
{
   const char *ident;
} video_shader_ctx_ident_t;

extern const shader_backend_t gl_glsl_backend;
extern const shader_backend_t hlsl_backend;
extern const shader_backend_t gl_cg_backend;
extern const shader_backend_t shader_null_backend;

/**
 * shader_ctx_find_driver:
 * @ident                   : Identifier of shader context driver to find.
 *
 * Finds shader context driver and initializes.
 *
 * Returns: shader context driver if found, otherwise NULL.
 **/
const shader_backend_t *shader_ctx_find_driver(const char *ident);

unsigned video_shader_driver_get_prev_textures(void);

bool video_shader_driver_ctl(enum video_shader_driver_ctl_state state, void *data);

#ifdef __cplusplus
}
#endif

#endif

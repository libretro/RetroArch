/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2019 - Hans-Kristian Arntzen
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

#ifndef SHADER_GL_CORE_H
#define SHADER_GL_CORE_H

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <glsym/glsym.h>

#include "glslang_util.h"

RETRO_BEGIN_DECLS

typedef struct gl3_filter_chain gl3_filter_chain_t;

struct gl3_filter_chain_texture
{
   GLuint image;
   unsigned width;
   unsigned height;
   unsigned padded_width;
   unsigned padded_height;
   GLenum format;
};

struct gl3_viewport
{
   GLint x;
   GLint y;
   GLsizei width;
   GLsizei height;
};

struct gl3_filter_chain_pass_info
{
   /* Maximum number of mip-levels to use. */
   unsigned max_levels;

   float scale_x;
   float scale_y;
   /* Ignored for the last pass, swapchain info 
    * will be used instead. */
   GLenum rt_format;
   /* For the last pass, make sure VIEWPORT scale
    * with scale factors of 1 are used. */
   enum glslang_filter_chain_scale scale_type_x;
   enum glslang_filter_chain_scale scale_type_y;
   /* The filter to use for source in this pass. */
   enum glslang_filter_chain_filter source_filter;
   enum glslang_filter_chain_filter mip_filter;
   enum glslang_filter_chain_address address;
};

struct gl3_buffer_locations
{
   GLint flat_ubo_vertex;
   GLint flat_ubo_fragment;
   GLint flat_push_vertex;
   GLint flat_push_fragment;
   GLuint buffer_index_ubo_vertex;
   GLuint buffer_index_ubo_fragment;
};

gl3_filter_chain_t *gl3_filter_chain_new(void);

void gl3_filter_chain_free(
      gl3_filter_chain_t *chain);

void gl3_filter_chain_set_shader(
      gl3_filter_chain_t *chain,
      unsigned pass,
      GLenum shader_type,
      const uint32_t *spirv,
      size_t spirv_words);

void gl3_filter_chain_set_pass_info(
      gl3_filter_chain_t *chain,
      unsigned pass,
      const struct gl3_filter_chain_pass_info *info);

bool gl3_filter_chain_init(gl3_filter_chain_t *chain);

void gl3_filter_chain_set_input_texture(
      gl3_filter_chain_t *chain,
      const struct gl3_filter_chain_texture *texture);

void gl3_filter_chain_set_frame_count(
      gl3_filter_chain_t *chain,
      uint64_t count);

void gl3_filter_chain_set_frame_count_period(
      gl3_filter_chain_t *chain,
      unsigned pass,
      unsigned period);

void gl3_filter_chain_set_frame_direction(
      gl3_filter_chain_t *chain,
      int32_t direction);

void gl3_filter_chain_set_pass_name(
      gl3_filter_chain_t *chain,
      unsigned pass,
      const char *name);

void gl3_filter_chain_build_offscreen_passes(
      gl3_filter_chain_t *chain,
      const struct gl3_viewport *vp);

void gl3_filter_chain_build_viewport_pass(
      gl3_filter_chain_t *chain,
      const struct gl3_viewport *vp,
      const float *mvp);

gl3_filter_chain_t *gl3_filter_chain_create_default(
      enum glslang_filter_chain_filter filter);

gl3_filter_chain_t *gl3_filter_chain_create_from_preset(
      const char *path,
      enum glslang_filter_chain_filter filter);

struct video_shader *gl3_filter_chain_get_preset(
      gl3_filter_chain_t *chain);

void gl3_filter_chain_end_frame(gl3_filter_chain_t *chain);

GLuint gl3_cross_compile_program(
      const uint32_t *vertex,
      size_t vertex_size,
      const uint32_t *fragment,
      size_t fragment_size,
      struct gl3_buffer_locations *loc,
      bool flatten);

RETRO_END_DECLS

#endif

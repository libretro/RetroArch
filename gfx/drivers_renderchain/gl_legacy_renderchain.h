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

#ifndef __GL_RENDER_CHAIN_H
#define __GL_RENDER_CHAIN_H

#include <retro_common_api.h>
#include <libretro.h>

#include "../video_driver.h"
#include "../video_shader_parse.h"
#include "../common/gl_common.h"

RETRO_BEGIN_DECLS

void gl_renderchain_convert_geometry(gl_t *gl,
      struct video_fbo_rect *fbo_rect,
      struct gfx_fbo_scale *fbo_scale,
      unsigned last_width, unsigned last_max_width,
      unsigned last_height, unsigned last_max_height,
      unsigned vp_width, unsigned vp_height);

void gl_renderchain_bind_prev_texture(
      void *data,
      const struct video_tex_info *tex_info);

bool gl_renderchain_add_lut(const struct video_shader *shader,
      unsigned i, GLuint *textures_lut);

void gl_load_texture_data(
      uint32_t id_data,
      enum gfx_wrap_type wrap_type,
      enum texture_filter_type filter_type,
      unsigned alignment,
      unsigned width, unsigned height,
      const void *frame, unsigned base_size);

void gl_renderchain_render(gl_t *gl,
      video_frame_info_t *video_info,
      uint64_t frame_count,
      const struct video_tex_info *tex_info,
      const struct video_tex_info *feedback_info);

void gl_renderchain_init(gl_t *gl, unsigned fbo_width, unsigned fbo_height);

void gl_set_viewport(void *data, video_frame_info_t *video_info, 
      unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate);

void gl_deinit_hw_render(gl_t *gl);

void gl_deinit_fbo(gl_t *gl);

GLenum min_filter_to_mag(GLenum type);

void gl_renderchain_recompute_pass_sizes(gl_t *gl,
      unsigned width, unsigned height,
      unsigned vp_width, unsigned vp_height);

void gl_renderchain_start_render(gl_t *gl, video_frame_info_t *video_info);

void gl_check_fbo_dimensions(gl_t *gl);

void gl_renderchain_free(gl_t *gl);

bool gl_init_hw_render(gl_t *gl, unsigned width, unsigned height);

void gl_renderchain_viewport_info(void *data, struct video_viewport *vp);

bool gl_renderchain_read_viewport(void *data, uint8_t *buffer, bool is_idle);

void context_bind_hw_render(bool enable);

RETRO_END_DECLS

#endif


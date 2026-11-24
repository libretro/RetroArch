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

#ifndef __RARCH_GLSL_H
#define __RARCH_GLSL_H

#include <boolean.h>

#include "../video_defines.h"

void gl_glsl_set_context_type(bool core_profile, unsigned major, unsigned minor);
/* Latched before loading a preset; no viewer compilation on SDR contexts. */
void gl_glsl_set_viewer_hdr(bool enabled);

/* Select a load-time planar variant; NULL restores the source programs and
 * the texture bindings borrowed for the viewer draw. No compilation here. */
struct gl_planar_sample;
bool gl_glsl_set_media_planar(void *data, const struct gl_planar_sample *sample);

#ifdef HAVE_SHADERPIPELINE
#include "../gfx_display.h"

/* A menu effect's GLSL program, made by the GL drivers apart from the
 * shader backend, which comes and goes with the presets; with the
 * locations of what it reads, -1 for what it does not */
typedef struct gl_glsl_effect
{
   unsigned prog;
   int vertex_coord;
   int tex_coord;
   int color;
   int mvp;
   int time;
   int ribbon;
   int output_size;
} gl_glsl_effect_t;

/* From the dialect the context takes; false when the effect has none
 * or it does not build */
bool gl_glsl_effect_compile(gl_glsl_effect_t *fx,
      const gfx_display_effect_desc_t *desc);
#endif

/* Draws a gfx_display mesh from @vbo (and @ibo, when not 0: @count is
 * then indices, else vertices) with the texture bound to unit 0, under
 * @mvp and @tint, through a program of its own; the program the
 * caller had stays bound afterwards. False when that program could not
 * be made, and nothing was drawn. */
bool gl_glsl_draw_mesh(void *shader_data, unsigned vbo, unsigned ibo,
      unsigned count, bool strip, const float *mvp, const float *tint);

/* Whether the first pass can read a frame's views where they lie in its
 * texture, sampled with GL @filter and @wrap, as from a copy of each:
 * made the first time it is asked for that filter and wrap. */
bool gl_glsl_in_place(void *shader_data, unsigned filter, unsigned wrap);

/* With gl_glsl_in_place() true, the first pass reads the view at
 * @view_pos (VIDEO_POS_PACK) of @view_dims, from the top left of a frame
 * @frame_h rows high in a texture of @tex_dims (VIDEO_SCALE_PACK), its
 * rows bottom up when @flipped, until gl_glsl_in_place_end() */
void gl_glsl_in_place_begin(void *shader_data, bool flipped,
      unsigned view_pos, unsigned view_dims, unsigned frame_h,
      unsigned tex_dims);
void gl_glsl_in_place_end(void *shader_data);

#endif

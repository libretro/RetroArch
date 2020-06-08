/*
 * This file is part of vitaGL
 * Copyright 2017, 2018, 2019, 2020 Rinnegatamante
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 *shaders.h:
 *Header file for default shaders related stuffs
 */

#ifndef _SHADERS_H_
#define _SHADERS_H_

// Disable color buffer shader
SceGxmShaderPatcherId disable_color_buffer_fragment_id;
const SceGxmProgramParameter *disable_color_buffer_position;
SceGxmFragmentProgram *disable_color_buffer_fragment_program_patched;
const SceGxmProgramParameter *clear_depth;

// Clear shader
SceGxmShaderPatcherId clear_vertex_id;
SceGxmShaderPatcherId clear_fragment_id;
const SceGxmProgramParameter *clear_position;
const SceGxmProgramParameter *clear_color;
SceGxmVertexProgram *clear_vertex_program_patched;
SceGxmFragmentProgram *clear_fragment_program_patched;

// Color (RGBA/RGB) shader
SceGxmShaderPatcherId rgba_vertex_id;
SceGxmShaderPatcherId rgb_vertex_id;
SceGxmShaderPatcherId rgba_fragment_id;
const SceGxmProgramParameter *rgba_position;
const SceGxmProgramParameter *rgba_color;
const SceGxmProgramParameter *rgba_wvp;
const SceGxmProgramParameter *rgb_position;
const SceGxmProgramParameter *rgb_color;
const SceGxmProgramParameter *rgb_wvp;
SceGxmVertexProgram *rgba_vertex_program_patched;
SceGxmVertexProgram *rgba_u8n_vertex_program_patched;
SceGxmVertexProgram *rgb_vertex_program_patched;
SceGxmVertexProgram *rgb_u8n_vertex_program_patched;
SceGxmFragmentProgram *rgba_fragment_program_patched;
const SceGxmProgram *rgba_fragment_program;

// Texture2D shader
SceGxmShaderPatcherId texture2d_vertex_id;
SceGxmShaderPatcherId texture2d_fragment_id;
const SceGxmProgramParameter *texture2d_position;
const SceGxmProgramParameter *texture2d_texcoord;
const SceGxmProgramParameter *texture2d_wvp;
const SceGxmProgramParameter *texture2d_alpha_cut;
const SceGxmProgramParameter *texture2d_alpha_op;
const SceGxmProgramParameter *texture2d_tint_color;
const SceGxmProgramParameter *texture2d_tex_env;
const SceGxmProgramParameter *texture2d_clip_plane0;
const SceGxmProgramParameter *texture2d_clip_plane0_eq;
const SceGxmProgramParameter *texture2d_mv;
const SceGxmProgramParameter *texture2d_fog_mode;
const SceGxmProgramParameter *texture2d_fog_near;
const SceGxmProgramParameter *texture2d_fog_far;
const SceGxmProgramParameter *texture2d_fog_density;
const SceGxmProgramParameter *texture2d_fog_color;
const SceGxmProgramParameter *texture2d_tex_env_color;
SceGxmVertexProgram *texture2d_vertex_program_patched;
SceGxmFragmentProgram *texture2d_fragment_program_patched;
const SceGxmProgram *texture2d_fragment_program;

// Texture2D+RGBA shader
SceGxmShaderPatcherId texture2d_rgba_vertex_id;
SceGxmShaderPatcherId texture2d_rgba_fragment_id;
const SceGxmProgramParameter *texture2d_rgba_position;
const SceGxmProgramParameter *texture2d_rgba_texcoord;
const SceGxmProgramParameter *texture2d_rgba_wvp;
const SceGxmProgramParameter *texture2d_rgba_alpha_cut;
const SceGxmProgramParameter *texture2d_rgba_alpha_op;
const SceGxmProgramParameter *texture2d_rgba_color;
const SceGxmProgramParameter *texture2d_rgba_tex_env;
const SceGxmProgramParameter *texture2d_rgba_clip_plane0;
const SceGxmProgramParameter *texture2d_rgba_clip_plane0_eq;
const SceGxmProgramParameter *texture2d_rgba_mv;
const SceGxmProgramParameter *texture2d_rgba_fog_mode;
const SceGxmProgramParameter *texture2d_rgba_fog_near;
const SceGxmProgramParameter *texture2d_rgba_fog_far;
const SceGxmProgramParameter *texture2d_rgba_fog_density;
const SceGxmProgramParameter *texture2d_rgba_fog_color;
const SceGxmProgramParameter *texture2d_rgba_tex_env_color;
SceGxmVertexProgram *texture2d_rgba_vertex_program_patched;
SceGxmVertexProgram *texture2d_rgba_u8n_vertex_program_patched;
SceGxmFragmentProgram *texture2d_rgba_fragment_program_patched;
const SceGxmProgram *texture2d_rgba_fragment_program;

#endif

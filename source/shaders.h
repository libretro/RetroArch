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
extern SceGxmShaderPatcherId disable_color_buffer_fragment_id;
extern const SceGxmProgramParameter *disable_color_buffer_position;
extern SceGxmFragmentProgram *disable_color_buffer_fragment_program_patched;
extern const SceGxmProgramParameter *clear_depth;

// Clear shader
extern SceGxmShaderPatcherId clear_vertex_id;
extern SceGxmShaderPatcherId clear_fragment_id;
extern const SceGxmProgramParameter *clear_position;
extern const SceGxmProgramParameter *clear_color;
extern SceGxmVertexProgram *clear_vertex_program_patched;
extern SceGxmFragmentProgram *clear_fragment_program_patched;

// Color (RGBA/RGB) shader
extern SceGxmShaderPatcherId rgba_vertex_id;
extern SceGxmShaderPatcherId rgb_vertex_id;
extern SceGxmShaderPatcherId rgba_fragment_id;
extern const SceGxmProgramParameter *rgba_position;
extern const SceGxmProgramParameter *rgba_color;
extern const SceGxmProgramParameter *rgba_wvp;
extern const SceGxmProgramParameter *rgb_position;
extern const SceGxmProgramParameter *rgb_color;
extern const SceGxmProgramParameter *rgb_wvp;
extern SceGxmVertexProgram *rgba_vertex_program_patched;
extern SceGxmVertexProgram *rgba_u8n_vertex_program_patched;
extern SceGxmVertexProgram *rgb_vertex_program_patched;
extern SceGxmVertexProgram *rgb_u8n_vertex_program_patched;
extern SceGxmFragmentProgram *rgba_fragment_program_patched;
extern const SceGxmProgram *rgba_fragment_program;

// Texture2D shader
extern SceGxmShaderPatcherId texture2d_vertex_id;
extern SceGxmShaderPatcherId texture2d_fragment_id;
extern const SceGxmProgramParameter *texture2d_position;
extern const SceGxmProgramParameter *texture2d_texcoord;
extern const SceGxmProgramParameter *texture2d_wvp;
extern const SceGxmProgramParameter *texture2d_alpha_cut;
extern const SceGxmProgramParameter *texture2d_alpha_op;
extern const SceGxmProgramParameter *texture2d_tint_color;
extern const SceGxmProgramParameter *texture2d_tex_env;
extern const SceGxmProgramParameter *texture2d_clip_plane0;
extern const SceGxmProgramParameter *texture2d_clip_plane0_eq;
extern const SceGxmProgramParameter *texture2d_mv;
extern const SceGxmProgramParameter *texture2d_fog_mode;
extern const SceGxmProgramParameter *texture2d_fog_near;
extern const SceGxmProgramParameter *texture2d_fog_far;
extern const SceGxmProgramParameter *texture2d_fog_density;
extern const SceGxmProgramParameter *texture2d_fog_color;
extern const SceGxmProgramParameter *texture2d_tex_env_color;
extern SceGxmVertexProgram *texture2d_vertex_program_patched;
extern SceGxmFragmentProgram *texture2d_fragment_program_patched;
extern const SceGxmProgram *texture2d_fragment_program;

// Texture2D+RGBA shader
extern SceGxmShaderPatcherId texture2d_rgba_vertex_id;
extern SceGxmShaderPatcherId texture2d_rgba_fragment_id;
extern const SceGxmProgramParameter *texture2d_rgba_position;
extern const SceGxmProgramParameter *texture2d_rgba_texcoord;
extern const SceGxmProgramParameter *texture2d_rgba_wvp;
extern const SceGxmProgramParameter *texture2d_rgba_alpha_cut;
extern const SceGxmProgramParameter *texture2d_rgba_alpha_op;
extern const SceGxmProgramParameter *texture2d_rgba_color;
extern const SceGxmProgramParameter *texture2d_rgba_tex_env;
extern const SceGxmProgramParameter *texture2d_rgba_clip_plane0;
extern const SceGxmProgramParameter *texture2d_rgba_clip_plane0_eq;
extern const SceGxmProgramParameter *texture2d_rgba_mv;
extern const SceGxmProgramParameter *texture2d_rgba_fog_mode;
extern const SceGxmProgramParameter *texture2d_rgba_fog_near;
extern const SceGxmProgramParameter *texture2d_rgba_fog_far;
extern const SceGxmProgramParameter *texture2d_rgba_fog_density;
extern const SceGxmProgramParameter *texture2d_rgba_fog_color;
extern const SceGxmProgramParameter *texture2d_rgba_tex_env_color;
extern SceGxmVertexProgram *texture2d_rgba_vertex_program_patched;
extern SceGxmVertexProgram *texture2d_rgba_u8n_vertex_program_patched;
extern SceGxmFragmentProgram *texture2d_rgba_fragment_program_patched;
extern const SceGxmProgram *texture2d_rgba_fragment_program;

#endif

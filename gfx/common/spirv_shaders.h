/*  RetroArch - A frontend for libretro.
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

#ifndef __RARCH_SPIRV_SHADERS_H
#define __RARCH_SPIRV_SHADERS_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* SPIR-V that more than one driver builds from, kept once. */
#define SPIRV_SHADER(name) \
   extern const uint32_t spirv_##name[]; \
   extern const size_t spirv_##name##_size;

SPIRV_SHADER(alpha_blend_vert)
SPIRV_SHADER(alpha_blend_frag)
SPIRV_SHADER(alpha_blend_linear_frag)
SPIRV_SHADER(mesh_vert)
SPIRV_SHADER(font_frag)
SPIRV_SHADER(stereo_anaglyph_frag)
SPIRV_SHADER(stereo_interlaced_frag)
SPIRV_SHADER(stock_rect_frag)
SPIRV_SHADER(opaque_vert)
SPIRV_SHADER(opaque_frag)
SPIRV_SHADER(hdr_frag)
SPIRV_SHADER(pipeline_ribbon_vert)
SPIRV_SHADER(pipeline_ribbon_frag)
SPIRV_SHADER(pipeline_ribbon_simple_vert)
SPIRV_SHADER(pipeline_ribbon_simple_frag)
SPIRV_SHADER(pipeline_snow_simple_frag)
SPIRV_SHADER(pipeline_snow_frag)
SPIRV_SHADER(pipeline_bokeh_frag)
SPIRV_SHADER(pipeline_snowflake_frag)
SPIRV_SHADER(hdr_scrgb_vert)
SPIRV_SHADER(hdr_scrgb_frag)

#undef SPIRV_SHADER

/* The GLSL a module glcore builds its own pipelines from was compiled
 * from, for a build without SPIRV-Cross to print instead; NULL for any
 * other module. */
const char *spirv_shader_glsl(const uint32_t *spirv);

RETRO_END_DECLS

#endif

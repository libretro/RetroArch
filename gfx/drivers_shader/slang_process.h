/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2017 - Hans-Kristian Arntzen
 *  Copyright (C) 2014-2018 - Ali Bouhlel
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

#ifndef __GLSLANG_PROCESS_H__
#define __GLSLANG_PROCESS_H__

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#include "../video_shader_parse.h"
#include "../../retroarch.h"
#include "glslang_util.h"

/* Vulkan maximum texture bindings inside shader. 
 * D3D11 has a hard limit of 16 */
#define SLANG_NUM_BINDINGS 16

enum slang_semantic
{
   /* mat4, MVP */
   SLANG_SEMANTIC_MVP              = 0,
   /* vec4, viewport size of current pass */
   SLANG_SEMANTIC_OUTPUT           = 1,
   /* vec4, viewport size of final pass */
   SLANG_SEMANTIC_FINAL_VIEWPORT   = 2,
   /* uint, frame count with modulo */
   SLANG_SEMANTIC_FRAME_COUNT      = 3,
   /* int, frame direction */
   SLANG_SEMANTIC_FRAME_DIRECTION  = 4,
   /* uint, FrameTimeDelta */
   SLANG_SEMANTIC_FRAME_TIME_DELTA = 5,
   /* float, OriginalFPS */
   SLANG_SEMANTIC_ORIGINAL_FPS     = 6,
   /* uint, rotation */
   SLANG_SEMANTIC_ROTATION         = 7,
   /* float, OriginalAspect */
   SLANG_SEMANTIC_CORE_ASPECT      = 8,
   /* float, OriginalAspectRotated */
   SLANG_SEMANTIC_CORE_ASPECT_ROT  = 9,
   /* uint, sub frames per content frame */
   SLANG_SEMANTIC_TOTAL_SUBFRAMES  = 10,
   /* uint, current sub frame */
   SLANG_SEMANTIC_CURRENT_SUBFRAME = 11,
   /* uint, HDR mode: 0=off, 1=HDR10, 2=scRGB */
   SLANG_SEMANTIC_HDR              = 12,
   /* float, HDR Brightness in nits */
   SLANG_SEMANTIC_PAPER_WHITE_NITS = 13,
   /* float, Enable HDR scanlines */
   SLANG_SEMANTIC_SCANLINES        = 14,
   /* uint, HDR Scanline Subpixel Layout */
   SLANG_SEMANTIC_SUBPIXEL_LAYOUT  = 15,
   /* uint, Enable HDR colour boost */
   SLANG_SEMANTIC_EXPAND_GAMUT     = 16,
   /* float, Enable HDR Inverse Tonemapper */
   SLANG_SEMANTIC_INVERSE_TONEMAP  = 17,
   /* float, Enable HDR10 conversion */
   SLANG_SEMANTIC_HDR10            = 18,
   /* vec3, gyroscope XYZ */
   SLANG_SEMANTIC_GYROSCOPE             = 19,
   /* vec3, accelerometer XYZ */
   SLANG_SEMANTIC_ACCELEROMETER         = 20,
   /* vec3, accelerometer rest position XYZ */
   SLANG_SEMANTIC_ACCELEROMETER_REST    = 21,
   SLANG_NUM_BASE_SEMANTICS        = 22,

   /* float, user defined parameter, arrayed */
   SLANG_SEMANTIC_FLOAT_PARAMETER  = 23,

   SLANG_NUM_SEMANTICS,
   SLANG_INVALID_SEMANTIC          = -1
};

enum slang_stage
{
   SLANG_STAGE_VERTEX_MASK   = 1 << 0,
   SLANG_STAGE_FRAGMENT_MASK = 1 << 1
};

enum slang_constant_buffer
{
   SLANG_CBUFFER_UBO = 0,
   SLANG_CBUFFER_PC,
   SLANG_CBUFFER_MAX
};

typedef struct
{
   void*  ptr;
   size_t stride;
} data_map_t;

typedef struct
{
   void*  image;
   size_t image_stride;
   void*  size;
   size_t size_stride;
} texture_map_t;

typedef struct
{
   texture_map_t textures[SLANG_NUM_TEXTURE_SEMANTICS];
   void*         uniforms[SLANG_NUM_BASE_SEMANTICS];
} semantics_map_t;

typedef struct
{
   void*    data;
   unsigned size;
   unsigned offset;
   char     id[64];
} uniform_sem_t;

typedef struct
{
   void*              texture_data;
   enum gfx_wrap_type wrap;
   unsigned           filter;
   unsigned           stage_mask;
   unsigned           binding;
   char               id[64];
} texture_sem_t;

typedef struct
{
   unsigned       stage_mask;
   unsigned       binding;
   unsigned       size;
   int            uniform_count;
   uniform_sem_t* uniforms;
} cbuffer_sem_t;

typedef struct
{
   int            texture_count;
   texture_sem_t* textures;
   cbuffer_sem_t  cbuffers[SLANG_CBUFFER_MAX];
   enum glslang_format format;
} pass_semantics_t;

struct slang_texture_semantic_map
{
   enum slang_texture_semantic semantic;
   unsigned index;
};

struct slang_semantic_map
{
   enum slang_semantic semantic;
   unsigned index;
};

RETRO_BEGIN_DECLS

/* Utility function to implement the same parameter reflection
 * which happens in the slang backend.
 * This does preprocess over the input file to handle #includes and so on. */
bool slang_preprocess_parse_parameters(const char *shader_path,
      struct video_shader *shader);

bool slang_process(
      struct video_shader*   shader_info,
      unsigned               pass_number,
      enum rarch_shader_type dst_type,
      unsigned               version,
      const semantics_map_t* semantics_map,
      pass_semantics_t*      out);

RETRO_END_DECLS

#ifdef __cplusplus
#include <vector>
#include <string>
#include <unordered_map>
#include <spirv_cross.hpp>

struct glslang_parameter
{
   std::string id;
   std::string desc;
   float initial;
   float minimum;
   float maximum;
   float step;
};

struct glslang_meta
{
   std::vector<glslang_parameter> parameters;
   std::string name;
   glslang_format rt_format;

   glslang_meta()
   {
	   rt_format = SLANG_FORMAT_UNKNOWN;
   }
};

struct glslang_output
{
   std::vector<uint32_t> vertex;
   std::vector<uint32_t> fragment;
   glslang_meta meta;
};

bool glslang_compile_shader(const char *shader_path, glslang_output *output);

bool slang_preprocess_parse_parameters(glslang_meta& meta,
      struct video_shader *shader);

struct slang_semantic_location
{
   int ubo_vertex;
   int push_vertex;
   int ubo_fragment;
   int push_fragment;

   slang_semantic_location()
      : ubo_vertex(-1), push_vertex(-1),
        ubo_fragment(-1), push_fragment(-1) {}
};

struct slang_texture_semantic_meta
{
   size_t   ubo_offset;
   size_t   push_constant_offset;
   unsigned binding;
   uint32_t stage_mask;

   bool texture;
   bool uniform;
   bool push_constant;

   /* For APIs which need location information ala legacy GL.
    * API user fills this struct in. */
   slang_semantic_location location;

   slang_texture_semantic_meta()
      : ubo_offset(0), push_constant_offset(0),
        binding(0), stage_mask(0),
        texture(false), uniform(false), push_constant(false) {}
};

struct slang_semantic_meta
{
   size_t   ubo_offset;
   size_t   push_constant_offset;
   unsigned num_components;
   bool     uniform;
   bool     push_constant;

   /* For APIs which need location information ala legacy GL. */
   slang_semantic_location location;

   slang_semantic_meta()
      : ubo_offset(0), push_constant_offset(0),
        num_components(0), uniform(false), push_constant(false) {}
};

struct slang_reflection
{
   slang_reflection();

   size_t   ubo_size;
   size_t   push_constant_size;

   unsigned ubo_binding;
   uint32_t ubo_stage_mask;
   uint32_t push_constant_stage_mask;

   std::vector<slang_texture_semantic_meta>
      semantic_textures[SLANG_NUM_TEXTURE_SEMANTICS];
   slang_semantic_meta semantics[SLANG_NUM_SEMANTICS];
   std::vector<slang_semantic_meta> semantic_float_parameters;

   const std::unordered_map<std::string, slang_texture_semantic_map> *texture_semantic_map;
   const std::unordered_map<std::string, slang_texture_semantic_map> *texture_semantic_uniform_map;
   const std::unordered_map<std::string, slang_semantic_map>         *semantic_map;
   unsigned pass_number;
};

template <typename P>
static bool slang_set_unique_map(std::unordered_map<std::string, P> &m,
      const std::string &name, const P &p)
{
   typename std::unordered_map<std::string, P>::iterator itr = m.find(name);
   /* Alias already exists? */
   if (itr != m.end())
      return false;
   m[name] = p;
   return true;
}

bool slang_reflect_spirv(
      const std::vector<uint32_t> &vertex,
      const std::vector<uint32_t> &fragment,
      slang_reflection *reflection);

bool slang_reflect(
      const spirv_cross::Compiler &vertex_compiler,
      const spirv_cross::Compiler &fragment_compiler,
      const spirv_cross::ShaderResources &vertex,
      const spirv_cross::ShaderResources &fragment,
      slang_reflection *reflection);
#endif

#endif

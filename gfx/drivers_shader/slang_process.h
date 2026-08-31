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
   /* True when 'format' came from a #pragma format in the shader source,
    * false when it was derived from preset FBO flags.  Backends must only
    * apply last-pass HDR heuristics to a shader-declared format. */
   bool                explicit_format;
} pass_semantics_t;


/* ---- C reflection data model ------------------------------------- */

/* Longest stored name: a pass alias (63 chars) plus the
 * "FeedbackSize" suffix (12), or a LUT id (63) plus "Size". */
#define SLANG_NAME_MAP_NAME_MAX 80

typedef struct slang_texture_semantic_map_entry
{
   char name[SLANG_NAME_MAP_NAME_MAX];
   unsigned char name_len;
   enum slang_texture_semantic semantic;
   unsigned index;
} slang_texture_semantic_map_entry;

typedef struct slang_texture_semantic_name_map
{
   slang_texture_semantic_map_entry *entries; /* malloc'd, grows */
   size_t count;
   size_t cap;
} slang_texture_semantic_name_map;

typedef struct slang_semantic_map_entry
{
   char name[SLANG_NAME_MAP_NAME_MAX];
   unsigned char name_len;
   enum slang_semantic semantic;
   unsigned index;
} slang_semantic_map_entry;

typedef struct slang_semantic_name_map
{
   slang_semantic_map_entry *entries;         /* malloc'd, grows */
   size_t count;
   size_t cap;
} slang_semantic_name_map;

typedef struct slang_semantic_location
{
   int ubo_vertex;
   int push_vertex;
   int ubo_fragment;
   int push_fragment;
} slang_semantic_location;

typedef struct slang_texture_semantic_meta
{
   size_t   ubo_offset;
   size_t   push_constant_offset;
   unsigned binding;
   uint32_t stage_mask;

   bool texture;
   bool uniform;
   bool push_constant;

   /* For APIs which need location information ala legacy GL.
    * API user fills this struct in; initialized to -1. */
   slang_semantic_location location;
} slang_texture_semantic_meta;

typedef struct slang_semantic_meta
{
   size_t   ubo_offset;
   size_t   push_constant_offset;
   unsigned num_components;
   bool     uniform;
   bool     push_constant;

   /* For APIs which need location information ala legacy GL. */
   slang_semantic_location location;
} slang_semantic_meta;

typedef struct slang_texture_semantic_array
{
   slang_texture_semantic_meta *data;         /* malloc'd, grows */
   size_t size;
   size_t cap;
} slang_texture_semantic_array;

typedef struct slang_reflection
{
   size_t   ubo_size;
   size_t   push_constant_size;

   unsigned ubo_binding;
   uint32_t ubo_stage_mask;
   uint32_t push_constant_stage_mask;

   slang_texture_semantic_array
      semantic_textures[SLANG_NUM_TEXTURE_SEMANTICS];
   slang_semantic_meta semantics[SLANG_NUM_SEMANTICS];
   slang_semantic_meta *semantic_float_parameters; /* malloc'd, grows */
   size_t num_float_parameters;
   size_t cap_float_parameters;

   const slang_texture_semantic_name_map *texture_semantic_map;
   const slang_texture_semantic_name_map *texture_semantic_uniform_map;
   const slang_semantic_name_map         *semantic_map;
   unsigned pass_number;
} slang_reflection;

RETRO_BEGIN_DECLS

/* Compiled slang shader output.  Plain C data model: SPIR-V words are
 * malloc'd arrays with explicit word counts, parameters are a malloc'd
 * grow-array, and strings are fixed buffers sized to match the
 * video_shader_parameter fields they are ultimately copied into
 * (id[64]/desc[64] in video_shader_parse.h) and the pass alias[64]
 * they name.  Lifecycle: glslang_output_init() before first use,
 * glslang_output_free() when done.  glslang_output_free() releases the
 * arrays and re-initializes the structure to the empty state, so
 * calling it twice - or calling it after a failed compile - is safe. */
typedef struct glslang_parameter
{
   char  id[64];
   char  desc[64];
   float initial;
   float minimum;
   float maximum;
   float step;
} glslang_parameter;

typedef struct glslang_meta
{
   glslang_parameter *parameters;      /* malloc'd, grows on demand */
   size_t num_parameters;
   size_t cap_parameters;
   enum glslang_format rt_format;
   char name[64];                      /* '\0' terminated, empty if unset */
} glslang_meta;

typedef struct glslang_output
{
   uint32_t *vertex;                   /* malloc'd SPIR-V words */
   uint32_t *fragment;                 /* malloc'd SPIR-V words */
   size_t vertex_len;                  /* in words */
   size_t fragment_len;                /* in words */
   glslang_meta meta;
} glslang_output;

void glslang_output_init(glslang_output *output);
void glslang_output_free(glslang_output *output);

/* Append a parameter to @meta, growing the array as needed.
 * Returns false on allocation failure ONLY; duplicate checking
 * is the caller's responsibility. */
bool glslang_meta_add_parameter(glslang_meta *meta,
      const glslang_parameter *param);

/* Compile the .slang file at @shader_path into @output.
 * @output is zero-initialized by these functions at entry and fully
 * (re)populated; it must not hold live allocations when passed in.
 * On failure the output is left in the freed/empty state.  On success
 * the caller owns the result and must call glslang_output_free(). */
bool glslang_compile_shader(const char *shader_path,
      glslang_output *output);

/* As glslang_compile_shader(), but expands '#include' directives
 * through @include_cache (see glslang_include_cache_new).  A preset's
 * passes share helper files, so one cache across a filter chain's pass
 * loop reads each file once instead of once per pass.  A NULL cache
 * behaves exactly like the uncached call. */
bool glslang_compile_shader_cached(const char *shader_path,
      glslang_output *output, void *include_cache);

/* Merge parameters harvested into @meta into @shader, enforcing the
 * duplicate-must-match rule.  (Formerly a C++ overload of
 * slang_preprocess_parse_parameters.) */
bool slang_preprocess_parse_parameters_meta(const glslang_meta *meta,
      struct video_shader *shader);

/* Utility function to implement the same parameter reflection
 * which happens in the slang backend.
 * This does preprocess over the input file to handle #includes and so on. */
bool slang_preprocess_parse_parameters(const char *shader_path,
      struct video_shader *shader);

/* As slang_preprocess_parse_parameters(), but expands '#include'
 * directives through @include_cache (see glslang_include_cache_new).
 * Harvesting parameters walks every pass of a preset, and the passes
 * share helper files, so one cache across that walk avoids re-reading
 * them per pass.  A NULL cache behaves exactly like the uncached call. */
bool slang_preprocess_parse_parameters_cached(const char *shader_path,
      struct video_shader *shader, void *include_cache);

/* Name-map lifecycle.  set_unique appends name -> (semantic, index);
 * it fails on a duplicate name, an over-long name, or allocation
 * failure.  The optional @suffix is concatenated after @name (used
 * for the "Size"/"FeedbackSize" derived entries); pass NULL or ""
 * for none.  free() releases the entries and re-initializes, so
 * calling it on a zeroed map or twice is safe. */
bool slang_texture_semantic_name_map_set_unique(
      slang_texture_semantic_name_map *map,
      const char *name, const char *suffix,
      enum slang_texture_semantic semantic, unsigned index);
void slang_texture_semantic_name_map_free(
      slang_texture_semantic_name_map *map);
bool slang_semantic_name_map_set_unique(
      slang_semantic_name_map *map,
      const char *name, const char *suffix,
      enum slang_semantic semantic, unsigned index);
void slang_semantic_name_map_free(slang_semantic_name_map *map);

/* Reflection lifecycle: init() zeroes the structure, sizes the
 * non-arrayed texture semantics (Original, Source) to one element
 * and presets every GL location to -1; free() releases the arrays
 * and re-initializes, so double-free is safe.  A reflection must be
 * init()ed before slang_reflect_spirv() and free()d afterwards. */
bool slang_reflection_init(slang_reflection *reflection);
void slang_reflection_free(slang_reflection *reflection);

/* Reflect the two SPIR-V stages into @reflection.  The name-map
 * pointers and pass_number must be set by the caller after init(). */
bool slang_reflect_spirv(
      const uint32_t *vertex,   size_t vertex_len,
      const uint32_t *fragment, size_t fragment_len,
      slang_reflection *reflection);

bool slang_process(
      struct video_shader*   shader_info,
      unsigned               pass_number,
      enum rarch_shader_type dst_type,
      unsigned               version,
      const semantics_map_t* semantics_map,
      pass_semantics_t*      out);

RETRO_END_DECLS

#ifdef __cplusplus
/* The DirectX SAL-compat shims (dxgi_common.h / dxsdk_sal_compat.h)
 * #define annotation tokens such as __out / __in / __inout to nothing
 * so the bundled DXSDK headers parse. Those names collide with real
 * identifiers inside libstdc++ -- e.g. std::__convert_from_v() in
 * <bits/c++locale.h> has a parameter literally named __out -- so if a
 * SAL macro is still live when the STL headers below are parsed, the
 * token is erased and the header fails to compile (this only bites
 * under CXX_BUILD, where these .c files are compiled as C++ and pull
 * in <vector>/<string>/<unordered_map>). Undef them here, after the
 * DX headers have already consumed them and before the STL includes. */
#undef __in
#undef __out
#undef __inout
#undef __in_opt
#undef __out_opt
#undef __inout_opt
#undef __in_ecount
#undef __out_ecount
#undef __in_ecount_opt
#undef __out_ecount_opt
#undef __in_bcount
#undef __out_bcount
#undef __in_bcount_opt
#undef __out_bcount_opt
#undef __out_bcount_part
#undef __deref_out_ecount


/* Owns an include cache for a scope, so a filter chain's pass loop can
 * share one across every pass without having to free it on each error
 * exit.  Defined here rather than in each chain's translation unit
 * because the griffin build compiles the Vulkan and GLCore chains into
 * one TU, where two identical definitions are still a redefinition. */
struct glslang_include_cache_guard
{
   void *handle;
   glslang_include_cache_guard() : handle(glslang_include_cache_new()) {}
   ~glslang_include_cache_guard() { glslang_include_cache_free(handle); }
   glslang_include_cache_guard(const glslang_include_cache_guard&) = delete;
   glslang_include_cache_guard& operator=(const glslang_include_cache_guard&) = delete;
};
#endif

#endif

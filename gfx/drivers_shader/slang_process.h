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
#include <retro_inline.h>

#include "../video_shader_parse.h"
#include "../../retroarch.h"
#include "glslang_util.h"
#include "slang_rect.h"

/* Built with rslc and without SPIRV-Cross: rslc's tables and printed
 * text serve every pass and every driver pipeline. */
#if defined(HAVE_RSLC) && !defined(HAVE_SPIRV_CROSS)
#define SLANG_RSLC_ONLY
#endif

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
   /* uint, presents completed before this frame's first present. Advances
    * on every swap the display sees - core frames, BFI dark frames, shader
    * sub-frames and repeats alike - so a shader can key an effect to the
    * monitor's cadence rather than the core's. */
   SLANG_SEMANTIC_SWAP_COUNT            = 22,
   /* vec4s, where the frame lies in the texture bound for it; see
    * slang_rect.h */
   SLANG_SEMANTIC_ORIGINAL_RECT         = 23,
   SLANG_SEMANTIC_ORIGINAL_CLAMP        = 24,
   SLANG_SEMANTIC_ORIGINAL_TEXELS       = 25,
   /* float, PaperWhiteNits alias of BrightnessNits */
   SLANG_SEMANTIC_PAPER_WHITE_ALIAS    = 26,
   /* float, effective HDR target luminance in nits */
   SLANG_SEMANTIC_PEAK_NITS            = 27,
   SLANG_SEMANTIC_MAX_NITS_ALIAS       = 28,
   /* float, active HDR output as 0 or 1 */
   SLANG_SEMANTIC_ENABLE_HDR          = 29,
   /* float, minimum luminance in nits; zero when unknown */
   SLANG_SEMANTIC_MIN_NITS            = 30,
   /* uint, HDR curve: 0=rational */
   SLANG_SEMANTIC_TONEMAP_OPERATOR    = 31,
   /* uint, Original is native PQ Rec.2020 */
   SLANG_SEMANTIC_SOURCE_HDR          = 32,
   /* uint, native HDR ownership without creative curves or white scaling */
   SLANG_SEMANTIC_PASSTHROUGH         = 33,
   SLANG_NUM_BASE_SEMANTICS        = 34,

   /* float, user defined parameter, arrayed */
   SLANG_SEMANTIC_FLOAT_PARAMETER  = 34,

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

/* A driver that sets uniforms[SLANG_SEMANTIC_ORIGINAL_RECT] (and the
 * clamp's and texels') may bind the frame inside a larger texture:
 * slang_process() rewrites each pass to read it through them where it
 * can, as slang_rect.h describes. A first pass whose filter the preset
 * leaves unspecified is read as frame_smooth says. */
typedef struct
{
   texture_map_t textures[SLANG_NUM_TEXTURE_SEMANTICS];
   void*         uniforms[SLANG_NUM_BASE_SEMANTICS];
   bool          frame_smooth;
} semantics_map_t;

typedef struct
{
   void*    data;
   /* Or a shader parameter, whose value is read through
    * video_shader_param_get() at the upload */
   const struct video_shader_parameter *param;
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
   /* The pass reads the frame wherever it lies in its texture, through
    * the rect semantics, or does not read it: see semantics_map_t. */
   bool                frame_in_place;
   /* It was rewritten for an unspecified filter, as frame_smooth was
    * then: built again when that changes. */
   bool                frame_unspec;
   bool                frame_smooth;
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

struct slang_blob;

typedef struct glslang_output
{
   struct slang_blob *vertex_blob;     /* one reference each; the words
                                          below are views into them */
   struct slang_blob *fragment_blob;
   const uint32_t *vertex;             /* SPIR-V words */
   const uint32_t *fragment;
   char *vertex_text;                  /* malloc'd stage source for a driver that
                                          takes text, when the compiler printed
                                          one (see struct slang_text_target);
                                          NULL otherwise */
   char *fragment_text;
   size_t vertex_len;                  /* in words */
   size_t fragment_len;                /* in words */
   glslang_meta meta;
} glslang_output;

/* What a driver that compiles source text wants printed beside the
 * SPIR-V: the language (GLSL, GLSL ES, HLSL or MSL), its version (a
 * GLSL #version or an HLSL shader model, 40 / 50) and, for GLSL, how
 * the interface is named, since the GL driver binds attributes,
 * varyings, blocks and samplers by name; HLSL binds by register and
 * semantic, MSL by buffer, texture and sampler index.
 * Prefixes take the location or binding as a decimal suffix.  Only
 * rslc prints; with glslang the text pointers stay NULL and the driver
 * crosses the SPIR-V as it does today. */
struct slang_text_target
{
   unsigned    version;
   bool        es;
   bool        hlsl;
   bool        msl;
   const char *attr_prefix;
   const char *varying_prefix;
   const char *sampler_prefix;
   /* block names per stage: [0] vertex, [1] fragment */
   const char *ubo_block[2];
   const char *ubo_instance[2];
   const char *push_block[2];
   const char *push_instance[2];
   /* GLSL: each block one array of vec4 named as its block, as glcore's
    * own pipelines upload them (rslc_names.flatten) */
   bool        flatten;
};

/* Accumulate active feedback resources from the compiled stages. */
bool slang_last_pass_feedback(struct glslang_last_pass *state,
      const glslang_output *output);

void glslang_output_init(glslang_output *output);
void glslang_output_free(glslang_output *output);

/* Gives a stage of @output to @blob, taking the reference: the words
 * the output's readers see are the blob's.  NULL clears the stage. */
void glslang_output_set_blob(glslang_output *output, bool fragment,
      struct slang_blob *blob);

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

/* As glslang_compile_shader_cached(), and when rslc is the compiler
 * also prints each stage as @text asks, into output->vertex_text and
 * output->fragment_text.  A cache hit brings words only. */
bool glslang_compile_shader_text(const char *shader_path,
      glslang_output *output, void *include_cache,
      const struct slang_text_target *text);

#ifdef HAVE_RSLC
/* One stage of GLSL source - include-expanded, as a driver embeds its
 * own shaders - printed for @t by rslc; malloc'd, NULL with the reason
 * logged when it cannot be. */
char *slang_print_stage(const char *source, bool fragment,
      const struct slang_text_target *t);

/* One of a driver's own stages built by rslc from its GLSL source, as
 * a pass is: the module with its tables in *blob and, for @t, its text
 * in *text (malloc'd).  False with nothing held when it cannot be. */
bool slang_compile_builtin(const char *source, bool fragment,
      const struct slang_text_target *t, struct slang_blob **blob,
      char **text);

/* A pass's frame-rectangle remap (slang_rect.h) done by rslc on the
 * pass's own source: placed from @vs, @fs, the stages as compiled, it
 * hands back the stages it rewrote with their tables and, for @text,
 * their text; a stage left as it was comes back NULL. *verdict as
 * slang_rect_remap_pass() says. False when rslc cannot do it: the
 * caller rewrites the SPIR-V. */
bool slang_compile_remapped(const char *shader_path,
      const uint32_t *vs, size_t vs_words,
      const uint32_t *fs, size_t fs_words,
      bool source, bool linear, enum slang_rect_wrap wrap,
      const struct slang_text_target *text,
      enum slang_rect_result *verdict,
      struct slang_blob **vs_out, struct slang_blob **fs_out,
      char **vs_text, char **fs_text);
#endif

/* The compile of one pass on a thread that is not the chain's: it
 * reads, compiles and writes the cache as the chain's thread would,
 * but asks and tells the blob table nothing and prints no text.  @key
 * (33 bytes) gets the cache key of the compiler that made the stages.
 * The chain's thread then offers the output's blobs under it
 * (slang_blob_offer) and its own compile of the pass takes them. */
bool slang_compile_pass_detached(const char *shader_path,
      glslang_output *output, char *key);

/* Lists @vs and @fs under @key for the passes to come, holding one
 * reference to each until the chain has taken them or the offers ring
 * (64 passes) has moved on; takes the references passed.  Nothing is
 * listed, and both are released, when the key is listed already.  On
 * the thread that builds chains. */
void slang_blob_offer(const char *key, struct slang_blob *vs,
      struct slang_blob *fs);

/* Drops the references the offers ring holds. */
void slang_blob_offers_release(void);

/* While held - by each deferred load in progress - the slang compiles
 * reuse rslc arenas from a pool of blocks instead of allocating every
 * stage's arena afresh; the last drop frees the blocks.  Any thread. */
void slang_arena_pool_hold(void);
void slang_arena_pool_drop(void);

/* Merge parameters harvested into @meta into @shader, enforcing the
 * duplicate-must-match rule.  @pass is the pass @meta was harvested
 * from, and is recorded on each parameter this call adds, so that a
 * parameter is owned by the first pass declaring it. */
bool slang_preprocess_parse_parameters_meta(const glslang_meta *meta,
      struct video_shader *shader, unsigned pass);

/* Utility function to implement the same parameter reflection
 * which happens in the slang backend.
 * This does preprocess over the input file to handle #includes and so on. */
bool slang_preprocess_parse_parameters(const char *shader_path,
      struct video_shader *shader, unsigned pass);

/* As slang_preprocess_parse_parameters(), but expands '#include'
 * directives through @include_cache (see glslang_include_cache_new).
 * Harvesting parameters walks every pass of a preset, and the passes
 * share helper files, so one cache across that walk avoids re-reading
 * them per pass.  A NULL cache behaves exactly like the uncached call. */
bool slang_preprocess_parse_parameters_cached(const char *shader_path,
      struct video_shader *shader, unsigned pass, void *include_cache);

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

/* The same for a pass processed by slang_process(), whose LUT views
 * and sizes were mapped from @views and @sizes, @stride apart. */
static INLINE void slang_semantics_luts_read(const pass_semantics_t *sem,
      const void *views, const void *sizes, size_t stride, unsigned count,
      uint8_t *read)
{
   int i, j;
   uintptr_t v = (uintptr_t)views, z = (uintptr_t)sizes;
   uintptr_t n = (uintptr_t)stride * count;
   for (i = 0; i < sem->texture_count; i++)
   {
      uintptr_t t = (uintptr_t)sem->textures[i].texture_data;
      if (t >= v && t - v < n && !((t - v) % stride))
         read[(t - v) / stride] = 1;
   }
   for (j = 0; j < SLANG_CBUFFER_MAX; j++)
      for (i = 0; i < sem->cbuffers[j].uniform_count; i++)
      {
         uintptr_t u = (uintptr_t)sem->cbuffers[j].uniforms[i].data;
         if (u >= z && u - z < n && !((u - z) % stride))
            read[(u - z) / stride] = 1;
      }
}

/* Sets read[i] for each of the first @count LUTs the pass samples or
 * takes the size of; the others are left as they are. */
static INLINE void slang_reflection_luts_read(
      const slang_reflection *reflection, uint8_t *read, unsigned count)
{
   unsigned i;
   const slang_texture_semantic_array *luts =
      &reflection->semantic_textures[SLANG_TEXTURE_SEMANTIC_USER];
   for (i = 0; i < luts->size && i < count; i++)
      if (     luts->data[i].texture
            || luts->data[i].uniform
            || luts->data[i].push_constant)
         read[i] = 1;
}

/* Reflect the two SPIR-V stages into @reflection.  The name-map
 * pointers and pass_number must be set by the caller after init(). */
bool slang_reflect_spirv(
      const uint32_t *vertex,   size_t vertex_len,
      const uint32_t *fragment, size_t fragment_len,
      slang_reflection *reflection);

#if defined(HAVE_RSLC)
/* Reflect a pass from the tables its compiler wrote into each stage's
 * blob (struct rslc_reflect): the same tables, checks and messages as
 * slang_reflect_spirv() derives through SPIRV-Cross, without a SPIR-V
 * parse.  Both blobs must carry tables. */
bool slang_reflect_blobs(const struct slang_blob *vertex,
      const struct slang_blob *fragment, slang_reflection *reflection);
#endif

/* Reflect a pass: by its blobs' tables when both stages carry them, by
 * SPIRV-Cross on the words otherwise (glslang's modules, a stage
 * slang_rect rewrote). */
bool slang_reflect_pass(const struct slang_blob *vertex,
      const struct slang_blob *fragment, slang_reflection *reflection);

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

#endif

#endif

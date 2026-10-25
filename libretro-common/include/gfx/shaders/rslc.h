/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rslc.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_RSLC_H__
#define __LIBRETRO_SDK_RSLC_H__

/* rslc: slang (Vulkan GLSL) compiler for the libretro shader stack.
 *
 * Bytes in, bytes out.  A stage source that has already been
 * include-expanded and stage-split is compiled into one structured IR,
 * from which the caller asks for one destination: SPIR-V words (Vulkan
 * and the slang cache), printed GLSL / GLSL ES / HLSL / MSL, or the
 * reflection tables alone.  Reflection is always filled.
 *
 * This header stays free of platform headers so the compiler can be
 * amalgamated next to anything: stdint, stddef, boolean and
 * retro_common_api only.  The compiler never opens a file, never takes
 * a lock, never touches process-global mutable state, and is re-entrant:
 * any number of rslc_compile calls may run on different threads at once
 * with their own opts and result. */

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum rslc_stage
{
   RSLC_STAGE_VERTEX = 0,
   RSLC_STAGE_FRAGMENT
};

enum rslc_dst
{
   RSLC_DST_SPIRV = 0,   /* vulkan + slang_cache */
   RSLC_DST_GLSL,        /* opts.version = 330 / 450 ... */
   RSLC_DST_GLSL_ES,     /* opts.version = 300 / 310 */
   RSLC_DST_HLSL,        /* opts.version = 40 / 50 / 51 */
   RSLC_DST_MSL,         /* opts.version = 12 / 20 ... (major*10+minor) */
   RSLC_DST_GLSL_CAFE,   /* GLSL for the Wii U's offline CafeGLSL: version
                            150, the push block as a second uniform block,
                            a location on every in and out */
   RSLC_DST_REFLECT      /* tables only */
};

/* Scalar base of a reflected member or I/O slot. */
enum rslc_base
{
   RSLC_BASE_VOID = 0,
   RSLC_BASE_BOOL,
   RSLC_BASE_INT,
   RSLC_BASE_UINT,
   RSLC_BASE_FLOAT
};

#define RSLC_MAX_SAMPLERS 16

/* Reflected binding of a sampler or block declared without
 * layout(binding).  The caller decides whether that is an error. */
#define RSLC_NO_BINDING 0xffffffffu

/* Reflection tables are pointer-free and live inside the blob, so the
 * same bytes serve a fresh compile, a cache hit and a core's own
 * shader table without a second copy.  Every name is a byte offset
 * from the start of the blob to a NUL-terminated string. */
#define RSLC_STR(blob, off) ((const char *)(blob) + (off))

/* One member of the UBO or push-constant block, laid out as glslang
 * does: std140 for the uniform block, std430 for push constants
 * unless the block is declared std140.  The same numbers are decorated
 * on the emitted SPIR-V and printed into the cbuffer / constant block,
 * so a CPU overlay built from this table matches every backend. */
struct rslc_member
{
   uint32_t    name;     /* RSLC_STR offset */
   uint32_t    offset;   /* byte offset in the block's layout: std140
                            for the UBO, std430 for push constants
                            unless the block says std140 */
   uint32_t    size;     /* byte size in that layout, arrays included */
   uint32_t    array_len;/* 0 when not an array */
   uint8_t     base;     /* enum rslc_base */
   uint8_t     cols;     /* 1 for scalars and vectors */
   uint8_t     rows;     /* vector width or matrix rows */
   uint8_t     active;   /* read or written by code main reaches: what
                            SPIRV-Cross's active buffer ranges list,
                            and what a chain needs to upload */
};

struct rslc_block
{
   uint32_t    members;  /* byte offset of struct rslc_member[member_count] */
   uint32_t    name;     /* block type name; 0 when absent */
   uint32_t    instance; /* block instance name; 0 when absent */
   uint32_t    size;     /* bytes to the end of the last member: what
                            SPIRV-Cross's get_declared_struct_size
                            reports, with no tail padding */
   uint32_t    member_count;
   uint32_t    binding;  /* RSLC_NO_BINDING when not declared; unused for push */
   uint32_t    present;  /* 0 when the stage declares no such block */
   uint32_t    set;      /* descriptor set; RSLC_NO_BINDING when not declared */
};

struct rslc_sampler
{
   uint32_t    name;
   uint32_t    binding;  /* RSLC_NO_BINDING when not declared */
   uint32_t    set;      /* descriptor set; RSLC_NO_BINDING when not declared */
   uint32_t    active;   /* read by code main reaches, or handed to a
                            call there: what SPIRV-Cross's active
                            interface variables list */
};

struct rslc_io
{
   uint32_t    name;
   uint32_t    location;
   uint8_t     base;     /* enum rslc_base */
   uint8_t     cols;
   uint8_t     rows;
   uint8_t     flat;     /* interpolation qualifier */
};

/* The reflection section of a blob, at rslc_blob_hdr.refl_off.  All
 * array fields are byte offsets from the blob start. */
struct rslc_reflect
{
   struct rslc_block ubo;
   struct rslc_block push;
   uint32_t          samplers;      /* struct rslc_sampler[sampler_count] */
   uint32_t          inputs;        /* struct rslc_io[input_count] */
   uint32_t          outputs;       /* struct rslc_io[output_count] */
   uint32_t          sampler_count;
   uint32_t          input_count;
   uint32_t          output_count;
   uint32_t          pad[2];
};

/* Layout of result.blob.  The blob is one allocation: this header,
 * the reflection section and its strings, then the payload (SPIR-V
 * words or printed text).  It is what a caller caches to disk and what
 * a driver reads from; nothing in it is a pointer, so it is valid as
 * bytes wherever it lands.  All multi-byte fields are host-endian; a
 * cache is per machine. */
#define RSLC_BLOB_MAGIC   0x434c5352u  /* 'RSLC' little-endian */
#define RSLC_BLOB_VERSION 4u

struct rslc_blob_hdr
{
   uint32_t magic;
   uint32_t version;
   uint32_t total_len;   /* bytes, header included */
   uint32_t refl_off;    /* reflection tables, relative to blob start */
   uint32_t refl_len;
   uint32_t payload_off; /* 4-byte aligned for SPIR-V */
   uint32_t payload_len; /* bytes; text payloads carry a trailing NUL */
   uint8_t  stage;       /* enum rslc_stage */
   uint8_t  dst;         /* enum rslc_dst */
   uint8_t  pad[2];
};

/* Frame-rectangle remap: an IR pass that rewrites every read of the
 * named samplers so the stage reads a rectangle of whatever texture
 * is bound there as it would read a texture holding only that
 * rectangle - same texels, same filtering, same wrap at the
 * rectangle's edges, same textureSize().  The rectangle arrives in
 * three vec4 members the pass appends to the push block or the uniform
 * block, named by the caller and filled by the caller each frame:
 *
 *   rect   (w / W, h / H, x / W, y / H)
 *   clamp  (first and last texel centres of the rectangle, biased)
 *   texels (w, h, x, y)
 *
 * The compiler knows nothing of what the samplers mean; it is handed
 * names.  Runs before any backend, so every destination carries the
 * rewrite: each read becomes a call of a helper that reads the
 * rectangle as gfx/drivers_shader/slang_rect.c's rewrite of a SPIR-V
 * module does, operation for operation, and a function the samplers
 * are passed to is written again for each way they are.  The members
 * go after what the block holds, at the first multiple of 16 at or
 * past @offset (a padding member keeps that place), or into a block of
 * their own, "rarch_rect", when the stage has none; a uniform block at
 * another set and binding, a block member that is a struct, or a push
 * block left past RSLC_REMAP_PUSH_LIMIT refuse the stage, as does a
 * read the pass cannot keep exact (linear filtering with repeat or
 * mirror, projective sampling, the level queries).  A refused stage,
 * or one that does not read the samplers, compiles unrewritten and the
 * result says which, for the caller to treat as it likes.  A build
 * without the text printers (RSLC_NO_TEXT) refuses every stage. */

/* The largest push block the rewrite may leave (the least any Vulkan
 * device offers): past it, the caller places the rectangle in the
 * uniform block. */
#define RSLC_REMAP_PUSH_LIMIT 128

enum rslc_remap_where
{
   RSLC_REMAP_PUSH = 0,
   RSLC_REMAP_UBO
};

enum rslc_remap_wrap
{
   RSLC_REMAP_WRAP_EDGE = 0,
   RSLC_REMAP_WRAP_BORDER,
   RSLC_REMAP_WRAP_REPEAT,
   RSLC_REMAP_WRAP_MIRROR
};

enum rslc_remap_result
{
   RSLC_REMAP_UNCHANGED = 0,  /* the stage does not read those samplers */
   RSLC_REMAP_REWRITTEN,
   RSLC_REMAP_UNSUPPORTED
};

struct rslc_remap
{
   const char *const *samplers;  /* names whose reads are rewritten */
   const char *rect_name;
   const char *clamp_name;
   const char *texels_name;
   uint32_t    offset;           /* first byte the three vec4s may use */
   unsigned    sampler_count;
   unsigned    set;
   unsigned    binding;
   enum rslc_remap_where where;
   enum rslc_remap_wrap  wrap;
   bool        linear;
};

/* How the printed destinations name the interface.  A driver binds
 * attributes, varyings, blocks and samplers by name, so it chooses
 * them: a NULL entry keeps the source's name.  Prefixes take the
 * location (attributes, varyings) or the binding (samplers) as a
 * decimal suffix.  The push block has no instance in GLSL or HLSL
 * text; it is printed as a struct of push_block's name with one
 * uniform of push_instance's name, and members are reached through
 * it.
 *
 * flatten (GLSL and GLSL ES): each block is printed instead as one
 * array of vec4 named ubo_block or push_block, sized to the block, and
 * a member is read out of it at its layout offset - a matrix rebuilt
 * from its columns, an integer through floatBitsToInt / Uint - so the
 * caller uploads the block's bytes with one glUniform4fv.  A block
 * holding an array, a struct, a bool, or a member that does not sit
 * inside one vec4 (a column stride other than 16) is refused. */
struct rslc_names
{
   const char *attr_prefix;      /* vertex inputs */
   const char *varying_prefix;   /* vertex outputs = fragment inputs */
   const char *sampler_prefix;
   const char *ubo_block;        /* the uniform block type / instance */
   const char *ubo_instance;
   const char *push_block;
   const char *push_instance;
   bool        flatten;
};

/* Everything a compile needs from the caller.  Zero-initialise and
 * set what differs from the defaults.
 *
 * names: optional interface naming for the printed destinations.
 * remap: optional frame-rectangle remap, see struct rslc_remap.
 * alloc / free_fn / ud: allocator for the arena chunks and the result
 *   blob.  NULL means malloc / free.
 * arena / arena_len: optional caller-owned block the compiler uses for
 *   all intermediate state instead of allocating chunks.  A worker
 *   that compiles many stages hands the same block in every time.
 *   When it is too small the compile fails with a message rather than
 *   falling back to the allocator, so the caller sees the real need.
 * arena_cap: hard ceiling in bytes for arena use when no caller arena
 *   is given.  0 selects RSLC_ARENA_CAP_DEFAULT.
 * err / err_cap: optional buffer that receives a one-line message on
 *   failure.  The compiler never logs anywhere else.
 * version: GLSL #version, HLSL shader model or MSL version per dst.
 * es / fixup_clipspace / flip_vert_y: the flags slang_process passes
 *   into the printers today; honoured at print time, never in the IR. */
struct rslc_opts
{
   void *(*alloc)(void *ud, size_t len);
   void  (*free_fn)(void *ud, void *ptr);
   void  *ud;
   void  *arena;
   char  *err;
   const struct rslc_remap *remap;
   const struct rslc_names *names;
   size_t arena_len;
   size_t arena_cap;
   size_t err_cap;
   unsigned version;
   enum rslc_dst dst;
   bool es;
   bool fixup_clipspace;
   bool flip_vert_y;
};

#define RSLC_ARENA_CHUNK        (64u * 1024u)
#define RSLC_ARENA_CAP_DEFAULT  (16u * 1024u * 1024u)

/* Output of a compile.  blob is the caller's to keep, cache, or hand
 * to a driver; every other pointer points into it.  spirv is set for
 * RSLC_DST_SPIRV, text for the printed destinations, neither for
 * RSLC_DST_REFLECT.  Release with rslc_result_free(). */
struct rslc_result
{
   void                      *blob;
   const struct rslc_reflect *refl;        /* points into blob */
   const uint32_t            *spirv;       /* points into blob */
   const char                *text;        /* points into blob */
   size_t                     blob_len;
   size_t                     spirv_len;   /* words */
   size_t                     text_len;    /* bytes, excluding the NUL */
   enum rslc_remap_result     remap;       /* RSLC_REMAP_UNCHANGED without opts.remap */
};

/* Compile one include-expanded stage.  glsl need not be NUL-terminated;
 * len bytes are read.  Returns false with opts->err filled on failure;
 * result is untouched then. */
bool rslc_compile(const char *glsl, size_t len,
      enum rslc_stage stage,
      const struct rslc_opts *opts, struct rslc_result *result);

/* Read a SPIR-V module this compiler or glslang emitted for the slang
 * stack back to its reflection: the blocks and their members, the
 * samplers, the stage inputs and outputs, and the block members the
 * code the entry point reaches touches.  opts->dst is RSLC_DST_REFLECT
 * (tables only) or RSLC_DST_SPIRV (tables and a copy of the words);
 * bodies are not decompiled, so a printed destination is refused and
 * the caller prints from the GLSL.  A module for another stage, with
 * capabilities beyond Shader, with unstructured control flow, with a
 * block layout this compiler would not produce, or with malformed
 * instructions is refused; the caller recompiles from GLSL. */
bool rslc_compile_spirv(const uint32_t *words, size_t n,
      enum rslc_stage stage,
      const struct rslc_opts *opts, struct rslc_result *result);

/* View an existing blob (one this compiler produced, read back from
 * a cache) through a result without copying: validates the header and
 * every offset in it, then points result at the blob.  result->blob
 * is the caller's pointer back; rslc_result_free() must not be called
 * on a result filled this way unless the caller allocated the blob
 * with opts' allocator and wants it freed. */
bool rslc_blob_open(void *blob, size_t len, struct rslc_result *result);

void rslc_result_free(struct rslc_result *result,
      const struct rslc_opts *opts);

RETRO_END_DECLS

#endif

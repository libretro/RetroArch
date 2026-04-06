/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2017 - Hans-Kristian Arntzen
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

#ifndef GLSLANG_UTIL_H
#define GLSLANG_UTIL_H

#include <stdint.h>
#include <retro_common_api.h>
#include <retro_inline.h>

#include "../video_shader_parse.h"

typedef enum glslang_format
{
   SLANG_FORMAT_UNKNOWN = 0,

   /* 8-bit */
   SLANG_FORMAT_R8_UNORM,
   SLANG_FORMAT_R8_UINT,
   SLANG_FORMAT_R8_SINT,
   SLANG_FORMAT_R8G8_UNORM,
   SLANG_FORMAT_R8G8_UINT,
   SLANG_FORMAT_R8G8_SINT,
   SLANG_FORMAT_R8G8B8A8_UNORM,
   SLANG_FORMAT_R8G8B8A8_UINT,
   SLANG_FORMAT_R8G8B8A8_SINT,
   SLANG_FORMAT_R8G8B8A8_SRGB,

   /* 10-bit */
   SLANG_FORMAT_A2B10G10R10_UNORM_PACK32,
   SLANG_FORMAT_A2B10G10R10_UINT_PACK32,

   /* 16-bit */
   SLANG_FORMAT_R16_UINT,
   SLANG_FORMAT_R16_SINT,
   SLANG_FORMAT_R16_SFLOAT,
   SLANG_FORMAT_R16G16_UINT,
   SLANG_FORMAT_R16G16_SINT,
   SLANG_FORMAT_R16G16_SFLOAT,
   SLANG_FORMAT_R16G16B16A16_UINT,
   SLANG_FORMAT_R16G16B16A16_SINT,
   SLANG_FORMAT_R16G16B16A16_SFLOAT,

   /* 32-bit */
   SLANG_FORMAT_R32_UINT,
   SLANG_FORMAT_R32_SINT,
   SLANG_FORMAT_R32_SFLOAT,
   SLANG_FORMAT_R32G32_UINT,
   SLANG_FORMAT_R32G32_SINT,
   SLANG_FORMAT_R32G32_SFLOAT,
   SLANG_FORMAT_R32G32B32A32_UINT,
   SLANG_FORMAT_R32G32B32A32_SINT,
   SLANG_FORMAT_R32G32B32A32_SFLOAT,

   SLANG_FORMAT_MAX
} glslang_format;

/* Textures with built-in meaning. */
typedef enum slang_texture_semantic
{
   /* The input texture to the filter chain.
    * Canonical name: "Original". */
   SLANG_TEXTURE_SEMANTIC_ORIGINAL         = 0,

   /* The output from pass N - 1 if executing pass N, or ORIGINAL
    * if pass #0 is executed.
    * Canonical name: "Source".
    */
   SLANG_TEXTURE_SEMANTIC_SOURCE           = 1,

   /* The original inputs with a history back in time.
    * Canonical name: "OriginalHistory#", e.g. "OriginalHistory2" <- Two frames back.
    * "OriginalHistory0" is an alias for SEMANTIC_ORIGINAL.
    * Size name: "OriginalHistorySize#".
    */
   SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY = 2,

   /* The output from pass #N, where pass #0 is the first pass.
    * Canonical name: "PassOutput#", e.g. "PassOutput3".
    * Size name: "PassOutputSize#".
    */
   SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT      = 3,

   /* The output from pass #N, one frame ago where pass #0 is the first pass.
    * It is not valid to use the pass feedback from a pass which is not offscreen.
    * Canonical name: "PassFeedback#", e.g. "PassFeedback2".
    */
   SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK    = 4,

   /* Inputs from static textures, defined by the user.
    * There is no canonical name, and the only way to use these semantics are by
    * remapping.
    */
   SLANG_TEXTURE_SEMANTIC_USER             = 5,

   SLANG_NUM_TEXTURE_SEMANTICS,
   SLANG_INVALID_TEXTURE_SEMANTIC       = -1
} slang_texture_semantic;

typedef enum glslang_filter_chain_filter
{
   GLSLANG_FILTER_CHAIN_LINEAR  = 0,
   GLSLANG_FILTER_CHAIN_NEAREST = 1,
   GLSLANG_FILTER_CHAIN_COUNT
} glslang_filter_chain_filter;

typedef enum glslang_filter_chain_address
{
   GLSLANG_FILTER_CHAIN_ADDRESS_REPEAT               = 0,
   GLSLANG_FILTER_CHAIN_ADDRESS_MIRRORED_REPEAT      = 1,
   GLSLANG_FILTER_CHAIN_ADDRESS_CLAMP_TO_EDGE        = 2,
   GLSLANG_FILTER_CHAIN_ADDRESS_CLAMP_TO_BORDER      = 3,
   GLSLANG_FILTER_CHAIN_ADDRESS_MIRROR_CLAMP_TO_EDGE = 4,
   GLSLANG_FILTER_CHAIN_ADDRESS_COUNT
} glslang_filter_chain_address;

typedef enum glslang_filter_chain_scale
{
   GLSLANG_FILTER_CHAIN_SCALE_ORIGINAL,
   GLSLANG_FILTER_CHAIN_SCALE_SOURCE,
   GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT,
   GLSLANG_FILTER_CHAIN_SCALE_ABSOLUTE
} glslang_filter_chain_scale;

/* Single contiguous char buffer with '\0'-delimited
 * lines stored back-to-back.  An offset table provides O(1) indexed access.
 *
 * Memory layout of data[]:
 *   "line0\0line1\0line2\0"
 *
 * line_offsets[i] gives the byte position of the i-th line inside data[].
 * Each line is individually null-terminated.
 */
struct shader_line_buf
{
   char    *data;          /* Contiguous buffer, lines separated by '\0' */
   size_t   len;           /* Current used length (includes null terminators) */
   size_t   cap;           /* Allocated capacity of data[]                   */
   size_t  *line_offsets;  /* Byte offset of each line start within data[]   */
   size_t   num_lines;     /* Number of lines stored                         */
   size_t   lines_cap;     /* Allocated capacity for line_offsets[]           */
   bool _single_alloc;
};

RETRO_BEGIN_DECLS

static INLINE enum glslang_filter_chain_address rarch_wrap_to_address(
      enum gfx_wrap_type type)
{
   switch (type)
   {
      case RARCH_WRAP_BORDER:
         return GLSLANG_FILTER_CHAIN_ADDRESS_CLAMP_TO_BORDER;
      case RARCH_WRAP_REPEAT:
         return GLSLANG_FILTER_CHAIN_ADDRESS_REPEAT;
      case RARCH_WRAP_MIRRORED_REPEAT:
         return GLSLANG_FILTER_CHAIN_ADDRESS_MIRRORED_REPEAT;
      case RARCH_WRAP_EDGE:
      default:
         break;
   }

   return GLSLANG_FILTER_CHAIN_ADDRESS_CLAMP_TO_EDGE;
}

const char *glslang_format_to_string(glslang_format fmt);

enum glslang_format glslang_find_format(const char *fmt);

/* Initialize a shader_line_buf to empty state. Returns false on alloc failure. */
bool shader_line_buf_init(struct shader_line_buf *buf);

/* Free all memory owned by a shader_line_buf. */
void shader_line_buf_free(struct shader_line_buf *buf);

/* Return pointer to the null-terminated line at the given index.
 * Valid until the next append (which may realloc). */
const char *shader_line_buf_get(const struct shader_line_buf *buf, size_t index);

/* Reads a shader file and outputs its contents into a shader_line_buf.
   Takes the path of the shader file and appends each line of the file
   to the output buffer.
   If the root_file argument is set to true, it expects the first line of the file
   to be a valid '#version' string.
   Handles '#include' statements by recursively parsing included files
   and appending their contents.
   Returns a bool indicating if parsing was successful.
 */
bool glslang_read_shader_file(const char *path,
      struct shader_line_buf *output, bool root_file, bool is_optional);

bool slang_texture_semantic_is_array(enum slang_texture_semantic sem);

enum slang_texture_semantic slang_name_to_texture_semantic_array(
      const char *name, const char **names,
      unsigned *index);

unsigned glslang_num_miplevels(unsigned width, unsigned height);

RETRO_END_DECLS

#endif

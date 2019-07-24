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

#ifndef SLANG_REFLECTION_H_
#define SLANG_REFLECTION_H_

/* Textures with built-in meaning. */
enum slang_texture_semantic
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
};

enum slang_semantic
{
   /* mat4, MVP */
   SLANG_SEMANTIC_MVP             = 0,
   /* vec4, viewport size of current pass */
   SLANG_SEMANTIC_OUTPUT          = 1,
   /* vec4, viewport size of final pass */
   SLANG_SEMANTIC_FINAL_VIEWPORT  = 2,
   /* uint, frame count with modulo */
   SLANG_SEMANTIC_FRAME_COUNT     = 3,
   /* int, frame direction */
   SLANG_SEMANTIC_FRAME_DIRECTION = 4,
   SLANG_NUM_BASE_SEMANTICS,

   /* float, user defined parameter, arrayed */
   SLANG_SEMANTIC_FLOAT_PARAMETER = 5,

   SLANG_NUM_SEMANTICS,
   SLANG_INVALID_SEMANTIC         = -1
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

/* Vulkan minimum limit. */
#define SLANG_NUM_BINDINGS 16

#endif

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

#ifndef SLANG_RECT_H__
#define SLANG_RECT_H__

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* A preset's frame, read where it lies in a larger texture.
 *
 * slang_rect_remap() rewrites one compiled stage of a pass so that every
 * read of the frame - Original, OriginalHistory0 and, in the first pass,
 * Source - reads a rectangle of whatever texture is bound there as it
 * would read a texture holding just that rectangle: the same texels,
 * the same filtering, the same wrap at the rectangle's edges, the same
 * textureSize(). The rectangle comes in three vec4s appended to the
 * stage's push constants or uniform block (see struct slang_rect_place),
 * filled by the chain each frame:
 *
 *   SLANG_RECT_NAME_RECT    (w / W, h / H, x / W, y / H)
 *   SLANG_RECT_NAME_CLAMP   ((x + .5 + d) / W, (y + .5 + d) / H,
 *                            (x + w - .5 + d) / W, (y + h - .5 + d) / H)
 *   SLANG_RECT_NAME_TEXELS  (w, h, x, y)
 *
 * for the rectangle at texel (x, y), w by h, of a W by H texture, with
 * d = SLANG_RECT_CLAMP_BIAS. Given the whole texture the rewritten stage
 * reads what the original would, so a chain need not know which kind of
 * input it will get.
 *
 * How a read stays the same:
 * - linear filtering, clamp to edge: the coordinate is moved into the
 *   rectangle and clamped to just past its first and last texel
 *   centres, where the filter gives the texel beyond no weight;
 * - nearest filtering, any wrap: the texel the sampler would pick is
 *   fetched, repeat and mirror wrapping the coordinate before it is
 *   scaled as samplers do;
 * - gathers: the four texels, wrapped likewise, are fetched;
 * - texelFetch: moved by the rectangle's origin;
 * - textureSize: the rectangle's.
 * The texel picked where a coordinate falls exactly on a texel's edge is
 * the one the shader's float arithmetic gives; a sampler rounding the
 * other way there would differ (seen only for mirrored gathers).
 * Anything else - linear filtering with another wrap, depth compares,
 * projective sampling, a frame texture used in a way this does not
 * know - is reported unsupported, and the chain keeps its copy. */

#define SLANG_RECT_NAME_RECT   "RARCH_OriginalRect"
#define SLANG_RECT_NAME_CLAMP  "RARCH_OriginalClamp"
#define SLANG_RECT_NAME_TEXELS "RARCH_OriginalTexels"

/* In texels: a quarter of the finest weight step filtering takes, 1/256,
 * so that the clamped coordinate's weight on the texel beyond rounds to
 * nothing either way. */
#define SLANG_RECT_CLAMP_BIAS (1.0f / 1024.0f)

/* The largest push constant block the rewrite may leave: the least any
 * Vulkan device offers. */
#define SLANG_RECT_PUSH_LIMIT 128

enum slang_rect_wrap
{
   SLANG_RECT_WRAP_EDGE = 0,
   SLANG_RECT_WRAP_BORDER,
   SLANG_RECT_WRAP_REPEAT,
   SLANG_RECT_WRAP_MIRROR
};

enum slang_rect_result
{
   /* The stage does not read the frame: *out is untouched. */
   SLANG_RECT_UNCHANGED = 0,
   /* *out holds the rewritten stage, malloc'd. */
   SLANG_RECT_REWRITTEN,
   /* The stage reads the frame in a way that cannot be kept exact. */
   SLANG_RECT_UNSUPPORTED
};

/* Where the three vec4s go: the push block, or the uniform block at
 * @set, @binding, at the first multiple of 16 at or past @offset and
 * past what the stage's own block holds. Stages share the memory of
 * both, so the caller takes @offset past the ends of all of a pass's
 * stages; and as the vec4s may leave a push block no larger than
 * SLANG_RECT_PUSH_LIMIT, it takes the uniform block when they do not
 * fit there. A stage without the block named gets one. */
enum slang_rect_where
{
   SLANG_RECT_PUSH = 0,
   SLANG_RECT_UBO
};

struct slang_rect_place
{
   enum slang_rect_where where;
   unsigned set;
   unsigned binding;
   uint32_t offset;
};

/* Where the block @place names ends in a stage, in bytes: 0 for a stage
 * without it, ~0u for one whose layout this cannot read. */
uint32_t slang_rect_block_end(const uint32_t *in, size_t in_len,
      const struct slang_rect_place *place);

/* The set and binding of a stage's uniform block; false for a stage
 * without one. */
bool slang_rect_uniform_block(const uint32_t *in, size_t in_len,
      unsigned *set, unsigned *binding);

/* @source: the first pass, whose Source is the frame too.
 * @linear, @wrap: how the pass samples the frame. */
enum slang_rect_result slang_rect_remap(const uint32_t *in, size_t in_len,
      bool source, bool linear, enum slang_rect_wrap wrap,
      const struct slang_rect_place *place,
      uint32_t **out, size_t *out_len);

RETRO_END_DECLS

#endif

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

#ifndef __RARCH_PATCH_STREAM_H
#define __RARCH_PATCH_STREAM_H

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Incremental counterpart to the whole-buffer appliers in task_patch.c.
 *
 * The whole-buffer form needs the entire source resident before it can
 * start, so patching is a separate pass after the content has finished
 * loading.  This form consumes the source as consecutive chunks, which
 * lets the patch advance on the same task tick that produced the bytes -
 * the load, the decompression and the patching interleave instead of
 * running as three serial passes over the same data.
 *
 * Output is byte-identical to the corresponding whole-buffer applier,
 * and independent of where the chunk boundaries fall.
 *
 * Residency differs per format, because the formats differ in how they
 * address the source:
 *   IPS - source read strictly sequentially; nothing is retained.
 *   UPS - source consumed in lockstep with the target; only a small
 *         rolling carry is retained.
 *   BPS - SourceCopy seeks arbitrarily, so the source must be retained
 *         in full (as it is today).  The gain here is the overlap with
 *         the load, not reduced memory.
 *   xdelta - as BPS: a window names the source segment it needs, which
 *         may be anywhere, so the source is retained.
 *
 * Usage:
 *   ps = patch_stream_ips_open(patch, patch_len, source_len);
 *   ... per tick: patch_stream_feed(ps, chunk, chunk_len);
 *   patch_stream_finish(ps, &out, &out_len);
 *   patch_stream_free(ps);
 *
 * The patch file itself is always fully resident: patches are small and
 * are read before the content in every existing flow.
 */

typedef struct patch_stream patch_stream_t;

/* @patch     : the patch file, fully in memory; must outlive the stream.
 * @patch_len : its length in bytes.
 * @src_len   : the source length the caller will feed in total.
 *
 * Returns NULL if the patch is malformed at the header level (the same
 * structural checks the whole-buffer applier performs up front). */
patch_stream_t *patch_stream_ips_open(const uint8_t *patch, size_t patch_len,
      size_t src_len);
patch_stream_t *patch_stream_ups_open(const uint8_t *patch, size_t patch_len,
      size_t src_len);
patch_stream_t *patch_stream_bps_open(const uint8_t *patch, size_t patch_len,
      size_t src_len);
/* .xdelta (VCDIFF).  Like BPS, a window may name any part of the
 * source, so the source is retained in full and the gain is the
 * overlap with the load.  Built only when HAVE_XDELTA is set; returns
 * NULL otherwise, which the caller already treats as "not streamable". */
patch_stream_t *patch_stream_xdelta_open(const uint8_t *patch,
      size_t patch_len, size_t src_len);

/* Feed the next consecutive span of source bytes.  Chunks must arrive in
 * order and must not overlap; any sizes are accepted.  Returns the bytes
 * taken from this chunk. */
size_t patch_stream_feed(patch_stream_t *ps, const uint8_t *chunk, size_t len);

/* No more source will arrive.  Emits whatever tail depends only on the
 * patch, detaches the finished target into @out (caller frees) and its
 * length into @out_len.  Returns false if the patch was malformed in a
 * way only detectable during application, in which case @out is
 * untouched and the caller should fall back to the unpatched content. */
bool patch_stream_finish(patch_stream_t *ps, uint8_t **out, size_t *out_len);

/* True once the stream can no longer produce a result - a malformed
 * patch, or an allocation that failed.  A caller feeding a long source
 * can stop early on this rather than pushing the remaining megabytes
 * through a stream whose finish is already going to fail. */
bool patch_stream_failed(patch_stream_t *ps);

/* NULL-safe. */
void patch_stream_free(patch_stream_t *ps);

RETRO_END_DECLS

#endif

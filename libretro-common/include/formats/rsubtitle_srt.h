/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rsubtitle_srt.h).
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

#ifndef LIBRETRO_RSUBTITLE_SRT_H
#define LIBRETRO_RSUBTITLE_SRT_H

#include <stddef.h>
#include <stdint.h>
#include <retro_common_api.h>

typedef struct rsubtitle_srt rsubtitle_srt_t;

#define RSUBTITLE_SRT_MAX_INPUT ((size_t)16 * 1024 * 1024)

typedef struct rsubtitle_srt_cue
{
   int64_t start_us, end_us;
   const char *text;
   size_t length;
} rsubtitle_srt_cue_t;

typedef struct rsubtitle_srt_span
{
   int64_t start_us, end_us;
   size_t offset, length;
} rsubtitle_srt_span_t;

RETRO_BEGIN_DECLS

/* Consumes malloc-allocated UTF-8 input on success or failure.
 * Limits: 16 MiB input, 65536 cues, 128-byte headers and 65536-byte text.
 * Parsing and allocation belong to the loading worker. */
rsubtitle_srt_t *rsubtitle_srt_take(char *data, size_t size);
/* begin consumes the input too; iterate returns -1 on error, 0 while
 * pending and 1 when immutable queries become available. Each work
 * item handles at most one UTF-8 scalar, input byte or cue entry. */
rsubtitle_srt_t *rsubtitle_srt_begin(char *data, size_t size);
/* Consumes both malloc-allocated inputs, including on failure. Each span
 * names UTF-8 text followed by a NUL within size. No SRT text conversion.
 * Uses the same input/cue/text limits and incremental validation/indexing. */
rsubtitle_srt_t *rsubtitle_srt_begin_cues(char *data, size_t size,
      rsubtitle_srt_span_t *spans, size_t count);
int rsubtitle_srt_iterate(rsubtitle_srt_t *track, size_t work);
void rsubtitle_srt_free(rsubtitle_srt_t *track);
size_t rsubtitle_srt_count(const rsubtitle_srt_t *track);
const rsubtitle_srt_cue_t *rsubtitle_srt_cue(
      const rsubtitle_srt_t *track, size_t index);
/* The first active cue, or count. Later cues may overlap; stop when
 * start_us exceeds the requested time and ignore expired cues. */
size_t rsubtitle_srt_find(const rsubtitle_srt_t *track, int64_t time_us);
/* First active cue at or after first, or count. Bounded tree traversal,
 * including when a long cue overlaps many already expired cues. */
size_t rsubtitle_srt_find_from(const rsubtitle_srt_t *track,
      int64_t time_us, size_t first);

RETRO_END_DECLS

#endif

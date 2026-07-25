/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rflac.h).
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

/* FLAC decoder.
 *
 * The caller owns the bytes and hands them over; the decoder never reads
 * anything for itself. That suits the shapes this has to serve, which
 * differ only in where the bytes come from and whether a header precedes
 * them:
 *
 *   - a whole .flac file already in memory;
 *   - a stream reassembled from a container, where the header is the
 *     track's setup data and each packet is one raw frame;
 *   - a stream with no header at all, whose geometry is recorded
 *     elsewhere -- what a CHD's 'flac' and 'cdfl' hunks are.
 *
 * The first two used to reach the decoder through read callbacks, which
 * meant a container arm had to lay its packets end to end into one
 * contiguous buffer first and keep that copy for the life of the
 * decoder. Handing over each packet as it arrives removes both the
 * assembly and the copy, and the third shape stops needing a forged
 * header to exist at all.
 *
 * Usage follows <encodings/deflate.h>: point the decoder at some input
 * and some output, call process() until it stops asking for more.
 *
 *    f = rflac_new();
 *    rflac_set_in(f, header, header_len);
 *    while (rflac_process(f, NULL, NULL) == RFLAC_PROCESS_NEXT)
 *       ;
 *    rflac_set_out_s16(f, pcm, frames);
 *    for (each packet)
 *    {
 *       rflac_set_in(f, packet, packet_len);
 *       while (rflac_process(f, &read, &wrote) == RFLAC_PROCESS_NEXT)
 *          ;
 *    }
 *
 * Input is borrowed, not copied: whatever was handed to rflac_set_in()
 * must stay valid until process() reports that it has been consumed. A
 * frame split across two inputs is handled -- the leftover head is kept
 * internally until the rest arrives -- so a caller feeding arbitrary
 * chunks does not have to find frame boundaries itself.
 */

#ifndef __LIBRETRO_SDK_FORMAT_RFLAC_H
#define __LIBRETRO_SDK_FORMAT_RFLAC_H

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum
{
   RFLAC_PROCESS_ERROR = -2,
   RFLAC_PROCESS_END   =  1,   /* stream finished                      */
   RFLAC_PROCESS_NEXT  =  0    /* give more input, or more output room */
};

/* Ceilings the format itself imposes. */
#define RFLAC_MAX_CHANNELS      8
#define RFLAC_MAX_BLOCK_SIZE 65535

typedef struct rflac_ctx rflac_t;

/**
 * rflac_format_t:
 *
 * Stream geometry. Read out of the header where there is one, supplied
 * by the caller where there is not.
 *
 * @block_size is the frame count of a block, and is zero when a stream
 * does not fix one. A headerless stream must state it, because nothing
 * else records it.
 */
typedef struct rflac_format
{
   uint32_t sample_rate;
   uint32_t channels;
   uint32_t bits_per_sample;
   uint32_t block_size;
} rflac_format_t;

/**
 * rflac_new:
 *
 * Creates a decoder that expects a header before any audio: the `fLaC`
 * marker and its metadata blocks, which may arrive in the same input as
 * the first frames or separately.
 *
 * Returns: a decoder, or NULL on allocation failure.
 */
rflac_t *rflac_new(void);

/**
 * rflac_new_raw:
 * @fmt        : geometry the stream does not carry
 *
 * Creates a decoder for a stream that begins at its first frame, with
 * no marker and no metadata. @fmt must describe the stream exactly;
 * nothing in the data will contradict a wrong answer, so a mistake here
 * decodes to plausible noise rather than failing.
 *
 * Returns: a decoder, or NULL on bad geometry or allocation failure.
 */
rflac_t *rflac_new_raw(const rflac_format_t *fmt);

void rflac_free(rflac_t *f);

/**
 * rflac_set_in:
 * @f          : decoder
 * @in         : bytes, borrowed until consumed
 * @in_size    : how many
 *
 * Replaces the input span. Anything left unconsumed from a previous
 * span is dropped, so this is called when the previous one is spent, or
 * to abandon it deliberately.
 */
void rflac_set_in(rflac_t *f, const uint8_t *in, size_t in_size);

/**
 * rflac_set_out_s16:
 * @f          : decoder
 * @out        : interleaved output, channels * @out_frames samples
 * @out_frames : capacity in PCM frames
 *
 * Selects integer output and points it somewhere. Samples narrower than
 * 16 bits are shifted up, wider ones truncated, so the destination
 * format is the caller's choice rather than the stream's.
 */
void rflac_set_out_s16(rflac_t *f, int16_t *out, size_t out_frames);

/**
 * rflac_set_out_f32:
 *
 * As above, for float output normalised to [-1, 1).
 */
void rflac_set_out_f32(rflac_t *f, float *out, size_t out_frames);

/**
 * rflac_process:
 * @f          : decoder
 * @read       : receives input bytes consumed (may be NULL)
 * @wrote      : receives PCM frames produced (may be NULL)
 *
 * Consumes as much input and produces as much output as it can, then
 * returns. Work is bounded by one FLAC frame per call, so a caller
 * spreading decode across ticks gets a bound it can rely on without
 * knowing the stream.
 *
 * With no output set, header and metadata are still parsed, which is
 * how geometry is learned before buffers are sized.
 *
 * Returns: RFLAC_PROCESS_NEXT when suspended, RFLAC_PROCESS_END at the
 * end of the stream, or RFLAC_PROCESS_ERROR.
 */
int rflac_process(rflac_t *f, size_t *read, size_t *wrote);

/**
 * rflac_format:
 *
 * Returns: the geometry, or NULL before a header has been parsed. Always
 * available immediately for a decoder made by rflac_new_raw().
 */
const rflac_format_t *rflac_format(const rflac_t *f);

/**
 * rflac_total_frames:
 *
 * Returns: the stream length in PCM frames, or 0 when it is not
 * recorded -- which is normal rather than exceptional, since a stream
 * written to a pipe, and every stream carried in a container, states
 * zero. Callers must not treat 0 as an empty stream.
 */
uint64_t rflac_total_frames(const rflac_t *f);

/**
 * rflac_seek:
 * @f          : decoder
 * @frame      : PCM frame to reach
 * @byte_offset: receives where the caller should resume input
 *
 * Prepares a seek. The decoder cannot fetch anything, so it reports
 * where in the stream the caller should point its next rflac_set_in().
 * The offset comes from the stream's seek table, so the answer lands at
 * a frame boundary at or before @frame; decoding forward from there
 * covers the remainder, which rflac_tell() reports.
 *
 * Returns RFLAC_PROCESS_NEXT with @byte_offset set when the caller
 * should resume there. A caller whose source is not byte-addressable --
 * a container handing over packets -- cannot honour that, and should
 * rewind its own source and decode forward instead.
 *
 * Returns RFLAC_PROCESS_ERROR when the stream has no seek table, which
 * is the common case: a table is optional, and a stream carried in a
 * container never has one. That is not a failure of the seek, only of
 * this shortcut, and the caller's fallback is the same as above.
 */
int rflac_seek(rflac_t *f, uint64_t frame, uint64_t *byte_offset);

/**
 * rflac_seek_resumed:
 * @f          : decoder
 * @frame      : the PCM frame the resumed position corresponds to
 *
 * Tells the decoder that input has been re-pointed at the offset a
 * previous rflac_seek() reported, so it can resynchronise and report
 * position correctly from there.
 */
void rflac_seek_resumed(rflac_t *f, uint64_t frame);

/**
 * rflac_tell:
 *
 * Returns: the PCM frame index the next output will start at.
 */
uint64_t rflac_tell(const rflac_t *f);

/**
 * rflac_reset:
 *
 * Discards decoder state but keeps the geometry, so a stream can be
 * played again from the start without reparsing a header. Any input
 * span is dropped.
 */
void rflac_reset(rflac_t *f);

RETRO_END_DECLS

#endif

/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rmpeg4_video.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RMPEG4_VIDEO_H
#define __LIBRETRO_SDK_FORMAT_RMPEG4_VIDEO_H

/* MPEG-4 Part 2 video (ISO/IEC 14496-2) decoder: Simple and Advanced
 * Simple profiles, and the short video header (H.263 baseline) that a
 * Simple profile decoder takes as well.
 *
 * I, P, B and S (global motion compensation) VOPs; H.263 and MPEG
 * quantisation with the VOL's matrices; intra DC and AC prediction;
 * one or four vectors a macroblock, unrestricted, half or quarter
 * sample; interlaced VOPs (field DCT, field prediction, field direct
 * mode, alternate vertical scan); direct mode; resync markers, data
 * partitioning and reversible VLCs. Rectangular VOLs, 4:2:0, 8 bits.
 * Shapes, sprites other than GMC, scalability, NEWPRED and reduced
 * resolution VOPs are not decoded: their VOPs are counted as skipped.
 *
 * The bitstream is read where it lies: rmpeg4_video_decode() takes
 * the caller's access unit - one or more start code units, as a
 * container hands them out - and copies nothing out of it. A VOP runs
 * to the next start code or the end of what was passed, so an access
 * unit must not end inside a VOP.
 *
 * Frames come out in display order. With B-VOPs possible (no VOL
 * control parameters saying low_delay) an I, P or S-VOP is held back
 * until the next one, as the B-VOPs coded after it are shown before it;
 * rmpeg4_video_flush() releases the last. A VOP that is not coded
 * (vop_coded 0) gives no frame and leaves the references as they are.
 *
 * Planes for a size are allocated when a VOL (or short header) brings
 * it, and nothing after; automatic storage stays inside the 2 KiB that
 * tools/stack_budget.py allows.
 */

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>
#include <boolean.h>

RETRO_BEGIN_DECLS

typedef struct rmpeg4_video rmpeg4_video_t;

typedef struct
{
   const uint8_t *y;
   const uint8_t *cb;
   const uint8_t *cr;
   unsigned       width;        /* display size              */
   unsigned       height;
   unsigned       y_stride;     /* padded to macroblocks     */
   unsigned       c_stride;
   uint32_t       time;         /* in rmpeg4_video_time_resolution() ticks */
   uint8_t        coding_type;  /* 1 I, 2 P, 3 B, 4 S        */
   uint8_t        interlaced;
   uint8_t        top_field_first;
   uint64_t       tag;          /* rmpeg4_video_set_tag's when the VOP was decoded */
} rmpeg4_video_frame_t;

rmpeg4_video_t *rmpeg4_video_init(void);
void            rmpeg4_video_free(rmpeg4_video_t *v);

/* Forget the references and any held frame, as on a seek; the VOL
 * stays. */
void            rmpeg4_video_reset(rmpeg4_video_t *v);

/**
 * rmpeg4_video_decode:
 * @v    : the decoder
 * @data : start code units, read in place
 * @len  : their size
 * @used : receives the bytes taken
 * @out  : receives a frame
 *
 * Decodes units until a frame is ready or @data is used up. Call again
 * with what is left (@data + *used) until it returns 0 with nothing
 * left: an access unit can hold more than one VOP (a packed bitstream).
 *
 * Returns: 1 with *out filled - its planes stay valid until the next
 * call - or 0.
 */
int  rmpeg4_video_decode(rmpeg4_video_t *v, const uint8_t *data, size_t len,
      size_t *used, rmpeg4_video_frame_t *out);

/* A caller's value for the VOPs the next decode calls decode, given
 * back in the frame each becomes (out->tag) however the frames are
 * reordered: an access unit's time stamps, say. 0 until set; a reset
 * keeps it. */
void rmpeg4_video_set_tag(rmpeg4_video_t *v, uint64_t tag);

/* The held frame at end of stream: 1 with *out filled, or 0 */
int  rmpeg4_video_flush(rmpeg4_video_t *v, rmpeg4_video_frame_t *out);

/* True once a VOL header (or a short header picture) gave the size */
bool     rmpeg4_video_has_vol(const rmpeg4_video_t *v);
unsigned rmpeg4_video_width(const rmpeg4_video_t *v);
unsigned rmpeg4_video_height(const rmpeg4_video_t *v);
/* vop_time_increment_resolution: ticks a second, 30000/1001 Hz for a
 * short header */
unsigned rmpeg4_video_time_resolution(const rmpeg4_video_t *v);
/* Pixel aspect ratio, 1:1 when not given */
void     rmpeg4_video_pixel_aspect(const rmpeg4_video_t *v,
      unsigned *num, unsigned *den);
/* VOPs not decoded: unsupported tools, or B-VOPs without references */
uint32_t rmpeg4_video_skipped(const rmpeg4_video_t *v);
/* Video packets (or VOPs) abandoned on a bitstream inconsistency */
uint32_t rmpeg4_video_errors(const rmpeg4_video_t *v);

RETRO_END_DECLS

#endif

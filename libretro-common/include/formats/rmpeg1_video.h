#ifndef __LIBRETRO_SDK_FORMAT_RMPEG1_VIDEO_H
#define __LIBRETRO_SDK_FORMAT_RMPEG1_VIDEO_H

/* MPEG-1 video (ISO/IEC 11172-2) and MPEG-2 video (H.262 | ISO/IEC
 * 13818-2, main profile) decoder.
 *
 * Written from ITU-T Rec. H.262 | ISO/IEC 13818-2, whose non-scalable syntax
 * is a superset of 11172-2 and whose Annex B carries the same variable length
 * code tables. Not derived from any existing implementation.
 *
 * Takes the video elementary stream -- what rmpeg1_ps hands out for
 * RMPEG1_PS_VIDEO packets -- and produces 4:2:0 planar frames.
 *
 * Decodes I, P and B pictures. Frames come out in display order: a B picture
 * is emitted as soon as it is decoded, while an I or P picture is held back
 * one reference, because the B pictures that follow it in the bitstream are
 * displayed before it. Call rmpeg1_video_flush() at end of stream to release
 * the last one.
 *
 * MPEG-2 (H.262) main profile, 4:2:0, up to 4095x2800: frame and field
 * pictures (the two fields of a frame come out as the one frame), frame,
 * field, 16x8 and dual-prime prediction, frame and field DCT, alternate
 * scan, Table B.15, the non-linear quantiser, 8 to 11-bit intra DC,
 * concealment motion vectors, quantiser matrix extensions. Checked against
 * the main profile streams of the ISO/IEC 13818-4 conformance suite
 * (samples/formats/mpeg2). 4:2:2, 4:4:4 and the scalable extensions are
 * not decoded: their pictures are counted as skipped.
 *
 * Pictures that cannot be reconstructed -- a P or B before the first I, on a
 * mid-stream entry -- are counted by rmpeg1_video_skipped() and stepped over
 * without disturbing the bitstream position.
 *
 * Buffers live in the decoder context; nothing a picture needs is allocated
 * once the planes for its size exist. Automatic storage stays well inside
 * the 2 KiB budget that tools/stack_budget.py enforces for an 8 KiB console
 * thread stack.
 */

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>
#include <boolean.h>

RETRO_BEGIN_DECLS

typedef struct rmpeg1_video rmpeg1_video_t;

typedef struct
{
   const uint8_t *y;
   const uint8_t *cb;
   const uint8_t *cr;
   unsigned       width;       /* display size            */
   unsigned       height;
   unsigned       y_stride;    /* padded to macroblocks   */
   unsigned       c_stride;
   unsigned       temporal_ref;
   uint8_t        coding_type; /* 1 = I, 2 = P, 3 = B, 4 = D */
   uint64_t       tag;         /* rmpeg1_video_set_tag's for the picture's start code */
} rmpeg1_video_frame_t;

rmpeg1_video_t *rmpeg1_video_init(void);
void            rmpeg1_video_free(rmpeg1_video_t *v);
void            rmpeg1_video_reset(rmpeg1_video_t *v);

/* Append elementary stream bytes. Returns bytes consumed; short only when
 * the internal window is full, in which case drain with _decode() first. */
size_t rmpeg1_video_write(rmpeg1_video_t *v, const uint8_t *data, size_t len);

/* A caller's value for the pictures whose start codes are in the bytes
 * written after this call, given back in the frame each becomes
 * (out->tag) however the frames are reordered: an access unit's time
 * stamps, say. A field pair takes its first field's. 0 until set;
 * rmpeg1_video_reset() forgets those pending. */
void rmpeg1_video_set_tag(rmpeg1_video_t *v, uint64_t tag);

/* Decode one picture. Returns 1 and fills *out when a frame is ready, 0 when
 * more input is needed. The frame's planes stay valid until the next call. */
int rmpeg1_video_decode(rmpeg1_video_t *v, rmpeg1_video_frame_t *out);

/* Signal that no more input is coming. A picture is normally known to be
 * complete only when the following start code arrives, so without this the
 * last picture of a stream is never emitted -- streams do not always end
 * with a sequence_end_code. Call it once, then drain with _decode(). */
void rmpeg1_video_flush(rmpeg1_video_t *v);

/* True once a sequence header has been parsed and the geometry is known. */
bool     rmpeg1_video_has_sequence(const rmpeg1_video_t *v);
unsigned rmpeg1_video_width(const rmpeg1_video_t *v);
unsigned rmpeg1_video_height(const rmpeg1_video_t *v);
/* Frame rate as an exact rational; 11172-2 codes 30000/1001 and friends. */
void     rmpeg1_video_framerate(const rmpeg1_video_t *v,
                                unsigned *num, unsigned *den);
/* 0 when unknown; otherwise the aspect_ratio_information code, 1..14. */
unsigned rmpeg1_video_aspect_code(const rmpeg1_video_t *v);

/* Count of pictures skipped because their coding type is not yet supported. */
uint32_t rmpeg1_video_skipped(const rmpeg1_video_t *v);
/* Count of slices abandoned on a bitstream inconsistency. */
uint32_t rmpeg1_video_errors(const rmpeg1_video_t *v);

RETRO_END_DECLS

#endif

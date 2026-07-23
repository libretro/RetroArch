/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image.h).
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

#ifndef __RARCH_IMAGE_CONTEXT_H
#define __RARCH_IMAGE_CONTEXT_H

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

#include <boolean.h>

RETRO_BEGIN_DECLS

enum image_process_code
{
   IMAGE_PROCESS_ERROR     = -2,
   IMAGE_PROCESS_ERROR_END = -1,
   IMAGE_PROCESS_NEXT      =  0,
   IMAGE_PROCESS_END       =  1,
   /* The transfer cannot advance until more of the file has been read
    * into the buffer (see image_transfer_set_avail); nothing was
    * consumed.  Only the video still decoders (WEBM, MP4) return
    * this, and only after the caller opts in by setting an avail
    * short of the full length. */
   IMAGE_PROCESS_WAIT      =  2
};

struct texture_compressed;  /* defined below, after enum image_type_enum */

struct texture_image
{
   uint32_t *pixels;
   unsigned width;
   unsigned height;
   bool supports_rgba;
   /* When true, ->pixels holds packed XRGB2101010 (10-bit per channel,
    * bits [29:20]=R [19:10]=G [9:0]=B) rather than 8-bit RGBA/BGRA. Only
    * honoured by drivers that advertise GFX_CTX_FLAGS_SCREEN_10BPC_SOURCE;
    * others fall back to an 8-bit copy via image_texture_narrow_10bit(). */
   bool pix10;
   /* Optional GPU-native compressed payload (BCn).  When non-NULL a
    * capable driver may upload it directly and leave ->pixels NULL;
    * image_texture_realize_rgba() decodes to ->pixels on demand for
    * drivers that cannot sample the format. */
   struct texture_compressed *compressed;
};

enum image_type_enum
{
   IMAGE_TYPE_NONE = 0,
   IMAGE_TYPE_PNG,
   IMAGE_TYPE_JPEG,
   IMAGE_TYPE_BMP,
   IMAGE_TYPE_TGA,
   IMAGE_TYPE_WEBP,
   IMAGE_TYPE_DDS,
   IMAGE_TYPE_WEBM,
   IMAGE_TYPE_MP4
};

#define IMAGE_MAX_MIPS 16

/* Several MIPS toolchains (RS90/Dingux, PSP, PS2, ...) predefine the bare
 * macro `mips` as 1 in GNU mode, which collides with the `mips` field of
 * struct texture_compressed below (and every `->mips` access downstream).
 * RA never uses the legacy macro (it uses __mips__), so drop it here. */
#ifdef mips
#undef mips
#endif

/* GPU-native compressed texture formats (block-compressed).  Producers
 * (currently the DDS loader) report one of these when a texture can be
 * uploaded to the GPU as-is; capable drivers skip the CPU decode to
 * RGBA8 entirely.  Extend with ETC2 or ASTC entries later. */
enum texture_gpu_format
{
   TEXTURE_GPU_FORMAT_NONE = 0,
   TEXTURE_GPU_FORMAT_BC1,       /* DXT1               */
   TEXTURE_GPU_FORMAT_BC2,       /* DXT3               */
   TEXTURE_GPU_FORMAT_BC3,       /* DXT5               */
   TEXTURE_GPU_FORMAT_BC4,       /* RGTC1 (1 channel)  */
   TEXTURE_GPU_FORMAT_BC5,       /* RGTC2 (2 channel)  */
   TEXTURE_GPU_FORMAT_BC6H_UF,   /* BPTC unsigned HDR  */
   TEXTURE_GPU_FORMAT_BC6H_SF,   /* BPTC signed HDR    */
   TEXTURE_GPU_FORMAT_BC7        /* BPTC LDR           */
};

/* Numeric mip layout reported by a loader without decoding.  Offsets are
 * byte offsets from the start of the source file buffer. */
struct image_gpu_layout
{
   enum texture_gpu_format format;
   unsigned                num_mips;
   unsigned                width[IMAGE_MAX_MIPS];
   unsigned                height[IMAGE_MAX_MIPS];
   size_t                  offset[IMAGE_MAX_MIPS];
   size_t                  size[IMAGE_MAX_MIPS];
};

struct texture_mip
{
   const void *data;
   unsigned    width;
   unsigned    height;
   size_t      size;
};

/* Owned, self-contained compressed payload carried on a texture_image.
 * storage holds a private copy of the source file (so the mip pointers
 * outlive the loader's buffer) and doubles as the input for the CPU
 * decode fallback in image_texture_realize_rgba(). */
struct texture_compressed
{
   void                    *storage;
   size_t                   storage_len;
   struct texture_mip      *mips;
   unsigned                 num_mips;
   enum texture_gpu_format  format;
   enum image_type_enum     type;      /* for the CPU-decode fallback */
};

enum image_type_enum image_texture_get_type(const char *path);

bool image_texture_load_buffer(struct texture_image *img,
   enum image_type_enum type, void *s, size_t len);

bool image_texture_load(struct texture_image *img, const char *path);
void image_texture_free(struct texture_image *img);

/* Force a CPU decode of a compressed texture_image into ->pixels (RGBA8).
 * No-op returning true if ->pixels is already present.  The fallback when
 * a driver cannot sample img->compressed->format. */
bool image_texture_realize_rgba(struct texture_image *img);

/* Narrow a texture_image whose ->pixels hold packed XRGB2101010 down to
 * 8-bit ARGB8888 in place (and clear ->pix10), for drivers that cannot sample
 * a 10-bit texture. No-op unless ->pix10 is set. */
void image_texture_narrow_10bit(struct texture_image *img);

/* Image transfer */

void image_transfer_free(void *data, enum image_type_enum type);

void *image_transfer_new(enum image_type_enum type);

bool image_transfer_start(void *data, enum image_type_enum type);

void image_transfer_set_buffer_ptr(
      void *data,
      enum image_type_enum type,
      void *ptr,
      size_t len);

int image_transfer_process(
      void *data,
      enum image_type_enum type,
      uint32_t **buf,
      size_t len,
      unsigned *width,
      unsigned *height,
      bool supports_rgba);

/* Report the GPU-native compressed mip layout of an already-started
 * transfer without decoding.  Returns false for formats that must be
 * CPU-decoded (non-DDS, uncompressed DDS, premultiplied, BC4/5/6H). */
bool image_transfer_get_gpu_layout(
      void *data,
      enum image_type_enum type,
      size_t len,
      struct image_gpu_layout *out);

bool image_transfer_iterate(void *data, enum image_type_enum type);

/* True when the last image_transfer_iterate stopped because the decoder
 * reached the resident byte frontier declared by
 * image_transfer_set_avail, rather than because the image finished.
 * Both cases return false from iterate, so a caller feeding a growing
 * read MUST consult this before concluding the transfer is complete -
 * treating a wall as completion decodes a partially-gathered image and
 * fails.  Always false for types without an avail wall. */
bool image_transfer_need_more(void *data, enum image_type_enum type);

/* Video-to-image transfers (WEBM, MP4) keep their decoder stream open
 * after a successful image_transfer_process, positioned just past the
 * first displayed frame.  This takes ownership of that stream so the
 * caller can continue the video as an animation without re-opening the
 * file; the returned handle is the same opaque stream the
 * image_transfer_anim_stream_* helpers operate on (free it with
 * image_transfer_anim_stream_free).  It BORROWS the buffer given via
 * image_transfer_set_buffer_ptr, which must outlive it.  Returns NULL
 * for other types, when no stream is held, or if it was already
 * detached; an undetached stream is closed by image_transfer_free. */
void *image_transfer_detach_anim_stream(void *data,
      enum image_type_enum type);

bool image_transfer_is_valid(void *data, enum image_type_enum type);

/* True if the last processed frame was written as packed XRGB2101010
 * (10-bit); only the video decoders can report this, for HDR sources. */
bool image_transfer_is_10bit(void *data, enum image_type_enum type);

/* Ask a video decoder to emit packed XRGB2101010 for 10-bit HDR sources
 * (still-image types ignore it). */
void image_transfer_set_want_10bit(void *data, enum image_type_enum type,
      int want);

/* Animation (animated images: WEBP, and the video track of WEBM).
 *
 * image_transfer_anim_new returns an opaque animation handle, or NULL
 * for still images / unsupported types, so a caller can try it first and
 * fall back to the single-frame image_transfer_* path. Frames are fully
 * composited RGBA canvases (memory order R,G,B,A); the caller advances
 * frames on its own clock using each frame's duration in milliseconds. */

void *image_transfer_anim_new(void *buf, size_t len,
      enum image_type_enum type);

void image_transfer_anim_free(void *anim, enum image_type_enum type);

int image_transfer_anim_num_frames(void *anim, enum image_type_enum type);

void image_transfer_anim_get_info(void *anim, enum image_type_enum type,
      unsigned *width, unsigned *height, int *loop_count);

const uint32_t *image_transfer_anim_get_frame(void *anim,
      enum image_type_enum type, int index, int *duration_ms);

/* Streaming animation: memory use independent of frame count. The
 * buffer passed to image_transfer_anim_stream_new is BORROWED and must
 * outlive the stream. next() returns the stream's internal canvas
 * (valid until the next call, do not free); NULL means end of one
 * pass - rewind to loop. */

void *image_transfer_anim_stream_new(void *buf, size_t len,
      enum image_type_enum type);

/* Progressive open over a partially-resident buffer: only the first
 * 'avail' bytes are guaranteed present.  On success the stream decodes
 * forward as far as 'avail' allows; raise it with
 * image_transfer_anim_stream_set_avail as more arrives.  need_more (may
 * be NULL) is set when the header/index needed to open is not yet
 * resident and a larger prefix should be retried.  Returns NULL for
 * types without a partial open (animated WEBP), so the caller keeps
 * the whole-buffer path for those. */
void *image_transfer_anim_stream_new_avail(void *buf, size_t len,
      size_t avail, enum image_type_enum type, int *need_more);

void image_transfer_anim_stream_free(void *stream,
      enum image_type_enum type);

void image_transfer_anim_stream_get_info(void *stream,
      enum image_type_enum type,
      unsigned *width, unsigned *height, int *num_frames, int *loop_count);

const uint32_t *image_transfer_anim_stream_next(void *stream,
      enum image_type_enum type, int *duration_ms);

/* Ask the stream to emit ARGB words (non-zero) or the default R,G,B,A
 * memory order (zero) from the next frame on.  Returns true when the
 * stream type honours the request (WEBM, MP4, and animated WEBP - the
 * video streams bake the order in their blit, WEBP in its sub-frame
 * decode stores, converting its persistent canvas once if the order
 * changes mid-animation) so the caller can skip its own R/B swizzle
 * pass; false for types that always emit the default order, where the
 * caller must keep converting. */
bool image_transfer_anim_stream_set_argb(void *stream,
      enum image_type_enum type, int argb);

/* For decoding a still from a file whose read is still in progress:
 * declare how many leading bytes of the buffer are valid.  Monotonic.
 * Only the video types (WEBM, MP4) honour it - their process step
 * returns IMAGE_PROCESS_WAIT instead of erroring at the wall - and it
 * is a no-op for every other type, whose transfers must keep seeing
 * fully-resident buffers. */
void image_transfer_set_avail(void *data, enum image_type_enum type,
      size_t avail);

/* The stream-level counterpart, for an animation stream adopted from
 * a still whose file read was still in flight: the demuxer's byte
 * wall was captured at open, and nothing else raises it once the
 * still's task is gone - without this, a stream adopted at N bytes
 * read treats N as the end of the file and loops the animation
 * there, forever.  Call with the full length once the read
 * completes (or progressively, should a streaming consumer appear).
 * No-op for types without a byte wall (animated WEBP). */
void image_transfer_anim_stream_set_avail(void *stream,
      enum image_type_enum type, size_t avail);

/* Bounded-memory streaming: media_floor is the fixed byte offset
 * where media data begins; consumed is the monotonic high-water byte
 * offset the decoder has read to.  A feeder keeps
 * [media_floor, consumed + lookahead) resident and can free below the
 * floor.  Both return 0 for a type with no byte cursor (animated
 * WEBP), which a caller reads as "not windowable - keep whole". */
size_t image_transfer_anim_stream_media_floor(void *stream,
      enum image_type_enum type);
size_t image_transfer_anim_stream_consumed(void *stream,
      enum image_type_enum type);

/* Companion to the above for WEBM, whose timestamp pre-scan is
 * truncated by the wall (timestamps live in the block headers): once
 * the buffer is complete, finish the scan so per-frame durations
 * match a stream opened over the whole file.  No-op for MP4 (its
 * scan reads the moov tables and is never truncated) and for types
 * without a scan. */
void image_transfer_anim_stream_complete_scan(void *stream,
      enum image_type_enum type, const void *buf, size_t len);

void image_transfer_anim_stream_rewind(void *stream,
      enum image_type_enum type);

RETRO_END_DECLS

#endif

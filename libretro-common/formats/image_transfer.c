/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image_transfer.c).
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

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <boolean.h>

#ifdef HAVE_RPNG
#include <formats/rpng.h>
#endif
#ifdef HAVE_RJPEG
#include <formats/rjpeg.h>
#endif
#ifdef HAVE_RTGA
#include <formats/rtga.h>
#endif
#ifdef HAVE_RBMP
#include <formats/rbmp.h>
#endif
#ifdef HAVE_RWEBP
#include <formats/rwebp.h>
#endif

#ifdef HAVE_RWEBM
#include <formats/rwebm_video.h>
#endif
#ifdef HAVE_RMP4
#include <formats/rmp4_video.h>
#endif
#ifdef HAVE_RDDS
#include <formats/rdds.h>
#endif

#include <formats/image.h>

/* Per-type dispatch for the still/animation decode front end.  Every
 * entry point below is a switch over enum image_type_enum that forwards
 * to one of the r<fmt> backends, so a caller can drive any supported
 * format through one API and read an unsupported combination off the
 * return value instead of testing the type itself.
 *
 * Each backend sits behind its own HAVE_R<FMT> build guard.  With the
 * guard off, image_transfer_new() returns NULL for that type and the
 * rest of the entry points degrade to the unsupported answer (NULL /
 * false / 0 / no-op), so no caller needs a compile-time type list.
 *
 * Y = forwarded to the backend, . = returns the unsupported answer,
 * s = constant stub (see the notes below):
 *
 *                                   PNG JPG BMP TGA WBP DDS WBM MP4
 *   new / free / set_buf_ptr        Y   Y   Y   Y   Y   Y   Y   Y
 *   start                           Y   Y   s   s   s   s   s   s
 *   is_valid                        Y   Y   s   s   s   s   s   s
 *   process                         Y   Y   Y   Y   Y   Y   Y   Y
 *   process slices (returns NEXT)   Y   Y   Y   Y   Y   Y   Y   Y
 *   iterate                         Y   Y   .   .   .   .   .   .
 *   need_more                       Y   Y   .   .   .   .   .   .
 *   set_avail                       Y   Y   .   .   .   .   Y   Y
 *   get_gpu_layout                  .   .   .   .   .   Y   .   .
 *   set_want_10bit / is_10bit       Y   .   .   .   .   .   Y   Y
 *   detach_anim_stream              .   .   .   .   .   .   Y   Y
 *   anim_* (whole buffer)           .   .   .   .   Y   .   .   .
 *   anim_stream_new                 Y   .   .   .   Y   .   Y   Y
 *   anim_stream_new_avail           Y   .   .   .   .   .   Y   Y
 *   anim_stream_free / _get_info /
 *     _next / _rewind / _set_argb   Y   .   .   .   Y   .   Y   Y
 *   anim_stream_set_avail           Y   .   .   .   .   .   Y   Y
 *   anim_stream_media_floor /
 *     _consumed                     .   .   .   .   .   .   Y   Y
 *   anim_stream_complete_scan       .   .   .   .   .   .   Y   .
 *
 * The gaps, in the order a caller runs into them:
 *
 * - start / is_valid are header probes.  Only PNG and JPEG parse a
 *   header incrementally and can therefore fail before decoding; the
 *   other backends validate inside their one-shot process step, so the
 *   stubs answer true and let process() report the failure.  Each stub
 *   is behind its format's HAVE_ guard like every other case, so a
 *   type compiled out answers false here as well.
 *
 * - there are two independent slicing mechanisms here, and the matrix
 *   rows for them do not line up.  iterate() is the incremental
 *   *parse* loop - walking chunk or marker structure before any pixels
 *   exist - and only PNG and JPEG have one; need_more() tells an
 *   iterate() that stalled at the resident-byte wall apart from one
 *   that finished.  Decode-phase slicing is the separate business of
 *   process() returning IMAGE_PROCESS_NEXT until the surface is
 *   complete, and every type does that.  So a false from iterate() does
 *   NOT mean the type decodes in one go - no type produces a whole
 *   surface from a single process() call any more, and the ones that
 *   answer false to iterate() are simply the ones with nothing to parse
 *   incrementally before the pixels start.
 *
 * - set_avail (the still-image byte wall) is honoured by PNG and JPEG,
 *   where it surfaces as need_more(), and by WEBM and MP4, where it
 *   surfaces as IMAGE_PROCESS_WAIT out of process().  BMP, TGA, WEBP
 *   and DDS have no partial-buffer decode and must be handed fully
 *   resident data.
 *
 * - 10-bit output is a property of the source, not of this layer: PNG
 *   (from 16-bit-per-channel RGB) and the two video types (from 10-bit
 *   HDR) can pack XRGB2101010.  JPEG is 8-bit by construction; BMP,
 *   TGA, WEBP and DDS are not wired for it.
 *
 * - get_gpu_layout is DDS only, and only for part of it - see
 *   rdds_get_gpu_layout() for the payloads it declines and hands back
 *   to the CPU decode path.
 *
 * - animation splits two ways.  Animated WEBP is the only type with
 *   the whole-buffer anim_* handle (decode everything, then index
 *   frames); APNG, animated WEBP, WEBM and MP4 all have the streaming
 *   form.  Of those, only WEBM and MP4 carry a byte cursor, so only
 *   they can be windowed (media_floor / consumed) or adopted from a
 *   still whose read is still in flight (detach_anim_stream).
 *   Animated WEBP additionally has no partial open, so
 *   anim_stream_new_avail() returns NULL for it and the caller keeps
 *   the whole-buffer path.  complete_scan() is WEBM only because only
 *   its timestamp pre-scan can be truncated by the wall.
 *
 * Deliberately not dispatched here:
 *
 * - Encoding.  rpng_save_image_*() and rbmp_save_image() are called
 *   directly by their users; there is no image_transfer_save().
 *
 * - Type sniffing.  image_texture_get_type() derives the enum from the
 *   path extension; nothing here inspects magic bytes.
 *
 * - The pre-decode readiness probes (rpng_header_ready,
 *   rjpeg_header_ready, rwebp_still_ready, rpng_is_apng).  They take a
 *   raw buffer rather than a handle, so tasks/task_image.c calls them
 *   before there is anything to dispatch on.
 *
 * - Format-specific side channels with no cross-type meaning, such as
 *   rpng_get_hdr_metadata() and the player-grade MP4 controls
 *   (rmp4_video_stream_seek_ms / _skip / _render / _span_ms), which
 *   exist for the WEBM core - it holds the concrete stream type and
 *   does not need this layer.
 */

void image_transfer_free(void *data, enum image_type_enum type)
{
   switch (type)
   {
      case IMAGE_TYPE_TGA:
#ifdef HAVE_RTGA
         rtga_free((rtga_t*)data);
#endif
         break;
      case IMAGE_TYPE_PNG:
         {
#ifdef HAVE_RPNG
            rpng_t *rpng = (rpng_t*)data;
            if (rpng)
               rpng_free(rpng);
#endif
         }
         break;
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         rjpeg_free((rjpeg_t*)data);
#endif
         break;
      case IMAGE_TYPE_BMP:
#ifdef HAVE_RBMP
         rbmp_free((rbmp_t*)data);
#endif
         break;
      case IMAGE_TYPE_WEBP:
#ifdef HAVE_RWEBP
         rwebp_free((rwebp_t*)data);
#endif
         break;
      case IMAGE_TYPE_WEBM:
#ifdef HAVE_RWEBM
         rwebm_video_free((rwebm_video_t*)data);
#endif
         break;
      case IMAGE_TYPE_MP4:
#ifdef HAVE_RMP4
         rmp4_video_free((rmp4_video_t*)data);
#endif
         break;
      case IMAGE_TYPE_DDS:
#ifdef HAVE_RDDS
         rdds_free((rdds_t*)data);
#endif
         break;
      case IMAGE_TYPE_NONE:
         break;
   }
}

void *image_transfer_new(enum image_type_enum type)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         return rpng_alloc();
#else
         break;
#endif
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         return rjpeg_alloc();
#else
         break;
#endif
      case IMAGE_TYPE_TGA:
#ifdef HAVE_RTGA
         return rtga_alloc();
#else
         break;
#endif
      case IMAGE_TYPE_BMP:
#ifdef HAVE_RBMP
         return rbmp_alloc();
#else
         break;
#endif
      case IMAGE_TYPE_WEBP:
#ifdef HAVE_RWEBP
         return rwebp_alloc();
#else
         break;
#endif
      case IMAGE_TYPE_WEBM:
#ifdef HAVE_RWEBM
         return rwebm_video_alloc();
#else
         break;
#endif
      case IMAGE_TYPE_MP4:
#ifdef HAVE_RMP4
         return rmp4_video_alloc();
#else
         break;
#endif
      case IMAGE_TYPE_DDS:
#ifdef HAVE_RDDS
         return rdds_alloc();
#else
         break;
#endif
      default:
         break;
   }

   return NULL;
}

bool image_transfer_start(void *data, enum image_type_enum type)
{

   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         if (!rpng_start((rpng_t*)data))
            break;
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         if (!rjpeg_start((rjpeg_t*)data))
            break;
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_TGA:
#ifdef HAVE_RTGA
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_BMP:
#ifdef HAVE_RBMP
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_WEBP:
#ifdef HAVE_RWEBP
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_WEBM:
#ifdef HAVE_RWEBM
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_MP4:
#ifdef HAVE_RMP4
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_DDS:
#ifdef HAVE_RDDS
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_NONE:
         break;
   }

   return false;
}

bool image_transfer_is_valid(
      void *data,
      enum image_type_enum type)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         return rpng_is_valid((rpng_t*)data);
#else
         break;
#endif
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         return rjpeg_is_valid((rjpeg_t*)data);
#else
         break;
#endif
      case IMAGE_TYPE_TGA:
#ifdef HAVE_RTGA
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_BMP:
#ifdef HAVE_RBMP
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_WEBP:
#ifdef HAVE_RWEBP
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_WEBM:
#ifdef HAVE_RWEBM
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_MP4:
#ifdef HAVE_RMP4
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_DDS:
#ifdef HAVE_RDDS
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_NONE:
         break;
   }

   return false;
}

void image_transfer_set_buffer_ptr(
      void *data,
      enum image_type_enum type,
      void *ptr,
      size_t len)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         rpng_set_buf_ptr((rpng_t*)data, (uint8_t*)ptr, len);
#endif
         break;
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         rjpeg_set_buf_ptr((rjpeg_t*)data, (uint8_t*)ptr, len);
#endif
         break;
      case IMAGE_TYPE_TGA:
#ifdef HAVE_RTGA
         rtga_set_buf_ptr((rtga_t*)data, (uint8_t*)ptr);
#endif
         break;
      case IMAGE_TYPE_BMP:
#ifdef HAVE_RBMP
         rbmp_set_buf_ptr((rbmp_t*)data, (uint8_t*)ptr);
#endif
         break;
      case IMAGE_TYPE_WEBP:
#ifdef HAVE_RWEBP
         rwebp_set_buf_ptr((rwebp_t*)data, (uint8_t*)ptr, len);
#endif
         break;
      case IMAGE_TYPE_WEBM:
#ifdef HAVE_RWEBM
         rwebm_video_set_buf_ptr((rwebm_video_t*)data, (uint8_t*)ptr, len);
#endif
         break;
      case IMAGE_TYPE_MP4:
#ifdef HAVE_RMP4
         rmp4_video_set_buf_ptr((rmp4_video_t*)data, (uint8_t*)ptr, len);
#endif
         break;
      case IMAGE_TYPE_DDS:
#ifdef HAVE_RDDS
         rdds_set_buf_ptr((rdds_t*)data, (uint8_t*)ptr);
#endif
         break;
      case IMAGE_TYPE_NONE:
         break;
   }
}

int image_transfer_process(
      void *data,
      enum image_type_enum type,
      uint32_t **buf, size_t len,
      unsigned *width, unsigned *height,
      bool supports_rgba)
{
   int ret = 0;

   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         ret = rpng_process_image(
               (rpng_t*)data,
               (void**)buf, len, width, height, supports_rgba);
         break;
#else
         break;
#endif
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         ret = rjpeg_process_image((rjpeg_t*)data,
               (void**)buf, len, width, height, supports_rgba);
         break;
#else
         break;
#endif
      case IMAGE_TYPE_TGA:
#ifdef HAVE_RTGA
         ret = rtga_process_image((rtga_t*)data,
               (void**)buf, len, width, height, supports_rgba);
         break;
#else
         break;
#endif
      case IMAGE_TYPE_BMP:
#ifdef HAVE_RBMP
         ret = rbmp_process_image((rbmp_t*)data,
               (void**)buf, len, width, height, supports_rgba);
         break;
#else
         break;
#endif
      case IMAGE_TYPE_WEBP:
#ifdef HAVE_RWEBP
         ret = rwebp_process_image((rwebp_t*)data,
               (void**)buf, len, width, height, supports_rgba);
         break;
#else
         break;
#endif
      case IMAGE_TYPE_WEBM:
#ifdef HAVE_RWEBM
         ret = rwebm_video_process_image((rwebm_video_t*)data,
               (void**)buf, len, width, height, supports_rgba);
         break;
#else
         break;
#endif
      case IMAGE_TYPE_MP4:
#ifdef HAVE_RMP4
         ret = rmp4_video_process_image((rmp4_video_t*)data,
               (void**)buf, len, width, height, supports_rgba);
         break;
#else
         break;
#endif
      case IMAGE_TYPE_DDS:
#ifdef HAVE_RDDS
         ret = rdds_process_image((rdds_t*)data,
               (void**)buf, len, width, height, supports_rgba);
         break;
#else
         break;
#endif
      case IMAGE_TYPE_NONE:
         break;
   }

#ifdef GEKKO
   /* Convert from linear ARGB to the Wii's tiled texture format.
    * Applied once when decoding finishes (IMAGE_PROCESS_END),
    * not during intermediate iterations. */
   if (ret == IMAGE_PROCESS_END && *buf && *width && *height)
   {
      unsigned tmp_pitch, width2, i;
      const uint16_t *src = NULL;
      uint16_t *dst       = NULL;
      /* (size_t) casts on width and height: pre-patch the uint32
       * multiplication width * height * 4 wrapped on 32-bit Wii
       * (Gekko is a 32-bit PowerPC) for any image with
       * width*height > 2^30, the malloc returned an undersized
       * buffer, and the memcpy below ran off the end.  This file
       * is reached only after rpng/rjpeg has already accepted the
       * image; on 32-bit (which is where this matters) those
       * decoders cap dimensions at 0x4000 which closes the
       * primitive at the source.  The casts here keep the
       * arithmetic safe regardless of upstream caps and on any
       * platform where image_transfer.c is compiled, including
       * future 64-bit Wii-class targets. */
      void *tmp           = malloc(
            (size_t)(*width) * (size_t)(*height) * sizeof(uint32_t));

      if (!tmp)
         return IMAGE_PROCESS_ERROR;

      memcpy(tmp, *buf,
            (size_t)(*width) * (size_t)(*height) * sizeof(uint32_t));
      tmp_pitch = ((*width) * sizeof(uint32_t)) >> 1;

      *width  &= ~3;
      *height &= ~3;
      width2   = (*width) << 1;
      src      = (const uint16_t*)tmp;
      dst      = (uint16_t*)*buf;

      for (i = 0; i < *height; i += 4, dst += 4 * width2)
      {
#define GX_BLIT_LINE_32(off) \
         { \
            unsigned x; \
            const uint16_t *tmp_src = src; \
            uint16_t       *tmp_dst = dst; \
            for (x = 0; x < width2 >> 3; x++, tmp_src += 8, tmp_dst += 32) \
            { \
               tmp_dst[  0 + off] = tmp_src[0]; \
               tmp_dst[ 16 + off] = tmp_src[1]; \
               tmp_dst[  1 + off] = tmp_src[2]; \
               tmp_dst[ 17 + off] = tmp_src[3]; \
               tmp_dst[  2 + off] = tmp_src[4]; \
               tmp_dst[ 18 + off] = tmp_src[5]; \
               tmp_dst[  3 + off] = tmp_src[6]; \
               tmp_dst[ 19 + off] = tmp_src[7]; \
            } \
            src += tmp_pitch; \
         }
         GX_BLIT_LINE_32(0)
         GX_BLIT_LINE_32(4)
         GX_BLIT_LINE_32(8)
         GX_BLIT_LINE_32(12)
#undef GX_BLIT_LINE_32
      }

      free(tmp);
   }
#endif

   return ret;
}

bool image_transfer_get_gpu_layout(
      void *data,
      enum image_type_enum type,
      size_t len,
      struct image_gpu_layout *out)
{
   switch (type)
   {
      case IMAGE_TYPE_DDS:
#ifdef HAVE_RDDS
         return rdds_get_gpu_layout((rdds_t*)data, len, out);
#else
         break;
#endif
      default:
         break;
   }
   return false;
}

/* Ask a decoder to emit packed XRGB2101010 instead of 8-bit RGBA.
 * Honoured by PNG (16-bit-per-channel RGB sources) and by the two video
 * types (10-bit HDR sources); a no-op for every other type, and for an
 * 8-bit source of an honouring type. */
void image_transfer_set_want_10bit(void *data, enum image_type_enum type,
      int want)
{
   switch (type)
   {
#ifdef HAVE_RPNG
      case IMAGE_TYPE_PNG:
         rpng_set_want_10bit((rpng_t*)data, want);
         break;
#endif
#ifdef HAVE_RWEBM
      case IMAGE_TYPE_WEBM:
         rwebm_video_set_want_10bit((rwebm_video_t*)data, want);
         break;
#endif
#ifdef HAVE_RMP4
      case IMAGE_TYPE_MP4:
         rmp4_video_set_want_10bit((rmp4_video_t*)data, want);
         break;
#endif
      default:
         break;
   }
}

/* Report whether the last processed frame was actually written as
 * packed XRGB2101010 rather than 8-bit RGBA, i.e. 10-bit was requested
 * and the source could supply it.  False for every type that cannot
 * produce it - see image_transfer_set_want_10bit above. */
bool image_transfer_is_10bit(void *data, enum image_type_enum type)
{
   switch (type)
   {
#ifdef HAVE_RPNG
      case IMAGE_TYPE_PNG:
         return rpng_is_10bit((const rpng_t*)data);
#endif
#ifdef HAVE_RWEBM
      case IMAGE_TYPE_WEBM:
         return rwebm_video_is_10bit((const rwebm_video_t*)data);
#endif
#ifdef HAVE_RMP4
      case IMAGE_TYPE_MP4:
         return rmp4_video_is_10bit((const rmp4_video_t*)data);
#endif
      default:
         break;
   }
   return false;
}

bool image_transfer_need_more(void *data, enum image_type_enum type)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         return rpng_need_more((const rpng_t*)data);
#else
         break;
#endif
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         return rjpeg_need_more((const rjpeg_t*)data);
#else
         break;
#endif
      default:
         break;
   }
   return false;
}

bool image_transfer_iterate(void *data, enum image_type_enum type)
{

   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         if (!rpng_iterate_image((rpng_t*)data))
            break;
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         if (!rjpeg_iterate_image((rjpeg_t*)data))
            break;
         return true;
#else
         break;
#endif
      /* One-shot decoders: nothing to iterate, the whole image is
       * produced by image_transfer_process(). */
      case IMAGE_TYPE_TGA:
      case IMAGE_TYPE_BMP:
      case IMAGE_TYPE_WEBP:
      case IMAGE_TYPE_DDS:
      case IMAGE_TYPE_WEBM:
      case IMAGE_TYPE_MP4:
      case IMAGE_TYPE_NONE:
         break;
   }

   return false;
}

void image_transfer_set_rgba(void *data, enum image_type_enum type,
      bool rgba)
{
   switch (type)
   {
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         rjpeg_set_out_rgba((rjpeg_t*)data, rgba);
#endif
         break;
      default:
         /* Only the JPEG decoder emits final pixels during the
          * transfer phase; the others take the order at process
          * time. */
         break;
   }
}

void image_transfer_set_avail(void *data, enum image_type_enum type,
      size_t avail)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         rpng_set_avail((rpng_t*)data, avail);
#endif
         break;
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         rjpeg_set_avail((rjpeg_t*)data, avail);
#endif
         break;
      case IMAGE_TYPE_WEBM:
#ifdef HAVE_RWEBM
         rwebm_video_set_avail((rwebm_video_t*)data, avail);
#endif
         break;
      case IMAGE_TYPE_MP4:
#ifdef HAVE_RMP4
         rmp4_video_set_avail((rmp4_video_t*)data, avail);
#endif
         break;
      default:
         break;
   }
}

void image_transfer_anim_stream_set_avail(void *stream,
      enum image_type_enum type, size_t avail)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         rpng_apng_stream_set_avail((rpng_apng_stream_t*)stream, avail);
#endif
         break;
      case IMAGE_TYPE_WEBM:
#ifdef HAVE_RWEBM
         rwebm_video_stream_set_avail((rwebm_video_stream_t*)stream,
               avail);
#endif
         break;
      case IMAGE_TYPE_MP4:
#ifdef HAVE_RMP4
         rmp4_video_stream_set_avail((rmp4_video_stream_t*)stream,
               avail);
#endif
         break;
      default:
         break;
   }
}

size_t image_transfer_anim_stream_media_floor(void *stream,
      enum image_type_enum type)
{
   switch (type)
   {
      case IMAGE_TYPE_WEBM:
#ifdef HAVE_RWEBM
         return rwebm_video_stream_media_floor(
               (rwebm_video_stream_t*)stream);
#else
         break;
#endif
      case IMAGE_TYPE_MP4:
#ifdef HAVE_RMP4
         return rmp4_video_stream_media_floor(
               (rmp4_video_stream_t*)stream);
#else
         break;
#endif
      default:
         break;
   }
   return 0;
}

size_t image_transfer_anim_stream_consumed(void *stream,
      enum image_type_enum type)
{
   switch (type)
   {
      case IMAGE_TYPE_WEBM:
#ifdef HAVE_RWEBM
         return rwebm_video_stream_consumed(
               (rwebm_video_stream_t*)stream);
#else
         break;
#endif
      case IMAGE_TYPE_MP4:
#ifdef HAVE_RMP4
         return rmp4_video_stream_consumed(
               (rmp4_video_stream_t*)stream);
#else
         break;
#endif
      default:
         break;
   }
   return 0;
}

void image_transfer_anim_stream_complete_scan(void *stream,
      enum image_type_enum type, const void *buf, size_t len){
   switch (type)
   {
      case IMAGE_TYPE_WEBM:
#ifdef HAVE_RWEBM
         rwebm_video_stream_complete_scan((rwebm_video_stream_t*)stream,
               (const uint8_t*)buf, len);
#endif
         break;
      case IMAGE_TYPE_MP4:
         /* The MP4 pre-scan reads the moov sample tables, which need
          * no media bytes: it is never truncated by the wall. */
         break;
      default:
         break;
   }
}

bool image_transfer_anim_stream_set_argb(void *stream,
      enum image_type_enum type, int argb)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         return rpng_apng_stream_set_argb((rpng_apng_stream_t*)stream, argb);
#else
         break;
#endif
      case IMAGE_TYPE_WEBP:
#ifdef HAVE_RWEBP
         rwebp_anim_stream_set_argb((rwebp_anim_stream_t*)stream, argb);
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_WEBM:
#ifdef HAVE_RWEBM
         rwebm_video_stream_set_argb((rwebm_video_stream_t*)stream, argb);
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_MP4:
#ifdef HAVE_RMP4
         rmp4_video_stream_set_argb((rmp4_video_stream_t*)stream, argb);
         return true;
#else
         break;
#endif
      default:
         break;
   }
   return false;
}

void *image_transfer_detach_anim_stream(void *data,
      enum image_type_enum type)
{
   switch (type)
   {
      case IMAGE_TYPE_WEBM:
#ifdef HAVE_RWEBM
         return rwebm_video_detach_stream((rwebm_video_t*)data);
#else
         break;
#endif
      case IMAGE_TYPE_MP4:
#ifdef HAVE_RMP4
         return rmp4_video_detach_stream((rmp4_video_t*)data);
#else
         break;
#endif
      default:
         break;
   }
   return NULL;
}

/* ===== Animation ===== *
 * Animated WEBP is the only type with the whole-buffer form directly
 * below; APNG, animated WEBP, WEBM and MP4 all have the streaming form
 * further down.  Both sets return NULL / false / 0 for every other
 * image type, so callers may attempt animation unconditionally and
 * fall back to the still-image path. */

void *image_transfer_anim_new(void *buf, size_t len,
      enum image_type_enum type)
{
   switch (type)
   {
      case IMAGE_TYPE_WEBP:
#ifdef HAVE_RWEBP
         return rwebp_anim_decode((const uint8_t*)buf, len);
#else
         break;
#endif
      default:
         break;
   }
   return NULL;
}

void image_transfer_anim_free(void *anim, enum image_type_enum type)
{
   switch (type)
   {
      case IMAGE_TYPE_WEBP:
#ifdef HAVE_RWEBP
         rwebp_anim_free((rwebp_anim_t*)anim);
#endif
         break;
      default:
         break;
   }
}

int image_transfer_anim_num_frames(void *anim, enum image_type_enum type)
{
   switch (type)
   {
      case IMAGE_TYPE_WEBP:
#ifdef HAVE_RWEBP
         return rwebp_anim_num_frames((const rwebp_anim_t*)anim);
#else
         break;
#endif
      default:
         break;
   }
   return 0;
}

void image_transfer_anim_get_info(void *anim, enum image_type_enum type,
      unsigned *width, unsigned *height, int *loop_count)
{
   switch (type)
   {
      case IMAGE_TYPE_WEBP:
#ifdef HAVE_RWEBP
         rwebp_anim_get_info((const rwebp_anim_t*)anim,
               width, height, loop_count);
#endif
         break;
      default:
         break;
   }
}

const uint32_t *image_transfer_anim_get_frame(void *anim,
      enum image_type_enum type, int index, int *duration_ms)
{
   switch (type)
   {
      case IMAGE_TYPE_WEBP:
#ifdef HAVE_RWEBP
         return rwebp_anim_get_frame((const rwebp_anim_t*)anim,
               index, duration_ms);
#else
         break;
#endif
      default:
         break;
   }
   return NULL;
}

/* ---- Streaming animation ---- */

void *image_transfer_anim_stream_new(void *buf, size_t len,
      enum image_type_enum type)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         /* APNG: returns NULL for a still PNG, which the caller reads
          * as "not animated" and keeps its static path. */
         return rpng_apng_stream_open((const uint8_t*)buf, len);
#else
         break;
#endif
      case IMAGE_TYPE_WEBP:
#ifdef HAVE_RWEBP
         return rwebp_anim_stream_open((const uint8_t*)buf, len);
#else
         break;
#endif
      case IMAGE_TYPE_WEBM:
#ifdef HAVE_RWEBM
         return rwebm_video_stream_open((const uint8_t*)buf, len);
#else
         break;
#endif
      case IMAGE_TYPE_MP4:
#ifdef HAVE_RMP4
         return rmp4_video_stream_open((const uint8_t*)buf, len);
#else
         break;
#endif
      default:
         break;
   }
   return NULL;
}

void *image_transfer_anim_stream_new_avail(void *buf, size_t len,
      size_t avail, enum image_type_enum type, int *need_more)
{
   if (need_more)
      *need_more = 0;
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         return rpng_apng_stream_open_avail((const uint8_t*)buf, len,
               avail, need_more);
#else
         break;
#endif
      case IMAGE_TYPE_WEBM:
#ifdef HAVE_RWEBM
         return rwebm_video_stream_open_avail((const uint8_t*)buf, len,
               avail, need_more);
#else
         break;
#endif
      case IMAGE_TYPE_MP4:
#ifdef HAVE_RMP4
         return rmp4_video_stream_open_avail((const uint8_t*)buf, len,
               avail, need_more);
#else
         break;
#endif
      /* Animated WEBP has no partial-buffer open (and is small enough
       * that windowing it buys nothing); callers fall back to the
       * whole-buffer path for it. */
      default:
         break;
   }
   return NULL;
}

void image_transfer_anim_stream_free(void *stream,
      enum image_type_enum type){
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         rpng_apng_stream_close((rpng_apng_stream_t*)stream);
#endif
         break;
      case IMAGE_TYPE_WEBP:
#ifdef HAVE_RWEBP
         rwebp_anim_stream_close((rwebp_anim_stream_t*)stream);
#endif
         break;
      case IMAGE_TYPE_WEBM:
#ifdef HAVE_RWEBM
         rwebm_video_stream_close((rwebm_video_stream_t*)stream);
#endif
         break;
      case IMAGE_TYPE_MP4:
#ifdef HAVE_RMP4
         rmp4_video_stream_close((rmp4_video_stream_t*)stream);
#endif
         break;
      default:
         break;
   }
}

void image_transfer_anim_stream_get_info(void *stream,
      enum image_type_enum type,
      unsigned *width, unsigned *height, int *num_frames, int *loop_count)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         rpng_apng_stream_get_info((const rpng_apng_stream_t*)stream,
               width, height, num_frames, loop_count);
#endif
         break;
      case IMAGE_TYPE_WEBP:
#ifdef HAVE_RWEBP
         rwebp_anim_stream_get_info((const rwebp_anim_stream_t*)stream,
               width, height, num_frames, loop_count);
#endif
         break;
      case IMAGE_TYPE_WEBM:
#ifdef HAVE_RWEBM
         rwebm_video_stream_get_info((const rwebm_video_stream_t*)stream,
               width, height, num_frames, loop_count);
#endif
         break;
      case IMAGE_TYPE_MP4:
#ifdef HAVE_RMP4
         rmp4_video_stream_get_info((const rmp4_video_stream_t*)stream,
               width, height, num_frames, loop_count);
#endif
         break;
      default:
         break;
   }
}

const uint32_t *image_transfer_anim_stream_next(void *stream,
      enum image_type_enum type, int *duration_ms)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         return rpng_apng_stream_next((rpng_apng_stream_t*)stream,
               duration_ms);
#else
         break;
#endif
      case IMAGE_TYPE_WEBP:
#ifdef HAVE_RWEBP
         return rwebp_anim_stream_next((rwebp_anim_stream_t*)stream,
               duration_ms);
#else
         break;
#endif
      case IMAGE_TYPE_WEBM:
#ifdef HAVE_RWEBM
         return rwebm_video_stream_next((rwebm_video_stream_t*)stream,
               duration_ms);
#else
         break;
#endif
      case IMAGE_TYPE_MP4:
#ifdef HAVE_RMP4
         return rmp4_video_stream_next((rmp4_video_stream_t*)stream,
               duration_ms);
#else
         break;
#endif
      default:
         break;
   }
   return NULL;
}

void image_transfer_anim_stream_rewind(void *stream,
      enum image_type_enum type)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         rpng_apng_stream_rewind((rpng_apng_stream_t*)stream);
#endif
         break;
      case IMAGE_TYPE_WEBP:
#ifdef HAVE_RWEBP
         rwebp_anim_stream_rewind((rwebp_anim_stream_t*)stream);
#endif
         break;
      case IMAGE_TYPE_WEBM:
#ifdef HAVE_RWEBM
         rwebm_video_stream_rewind((rwebm_video_stream_t*)stream);
#endif
         break;
      case IMAGE_TYPE_MP4:
#ifdef HAVE_RMP4
         rmp4_video_stream_rewind((rmp4_video_stream_t*)stream);
#endif
         break;
      default:
         break;
   }
}

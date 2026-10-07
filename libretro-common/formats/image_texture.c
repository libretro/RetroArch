/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image_texture.c).
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
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include <boolean.h>
#include <formats/image.h>
#include <formats/data_transfer.h>
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

enum image_type_enum image_texture_get_type(const char *path)
{
   /* We are comparing against a fixed list of file
    * extensions, the longest (jpeg) being 4 characters
    * in length. We therefore only need to extract the first
    * 5 characters from the extension of the input path
    * to correctly validate a match */
   size_t len;
   const char *ext = NULL;
   if (!path || *path == '\0')
      return IMAGE_TYPE_NONE;

   ext = strrchr(path, '.');
   if (!ext || (*(++ext) == '\0'))
      return IMAGE_TYPE_NONE;

   len = strlen(ext);

   /* All supported extensions are 3 or 4 characters */
   if (len < 3 || len > 4)
      return IMAGE_TYPE_NONE;

   /* Compare with inline lowering — avoids copy + tolower pass */
   switch (len)
   {
      case 3:
#ifdef HAVE_RPNG
         if ((ext[0] | 0x20) == 'p' &&
             (ext[1] | 0x20) == 'n' &&
             (ext[2] | 0x20) == 'g')
            return IMAGE_TYPE_PNG;
#endif
#ifdef HAVE_RJPEG
         if ((ext[0] | 0x20) == 'j' &&
             (ext[1] | 0x20) == 'p' &&
             (ext[2] | 0x20) == 'g')
            return IMAGE_TYPE_JPEG;
#endif
#ifdef HAVE_RBMP
         if ((ext[0] | 0x20) == 'b' &&
             (ext[1] | 0x20) == 'm' &&
             (ext[2] | 0x20) == 'p')
            return IMAGE_TYPE_BMP;
#endif
#ifdef HAVE_RTGA
         if ((ext[0] | 0x20) == 't' &&
             (ext[1] | 0x20) == 'g' &&
             (ext[2] | 0x20) == 'a')
            return IMAGE_TYPE_TGA;
#endif
#ifdef HAVE_RDDS
         if ((ext[0] | 0x20) == 'd' &&
             (ext[1] | 0x20) == 'd' &&
             (ext[2] | 0x20) == 's')
            return IMAGE_TYPE_DDS;
#endif
#ifdef HAVE_RMP4
         if ((ext[0] | 0x20) == 'm' &&
              ext[1]         == '4' &&
             (ext[2] | 0x20) == 'v')
            return IMAGE_TYPE_MP4;
         if ((ext[0] | 0x20) == 'm' &&
             (ext[1] | 0x20) == 'p' &&
              ext[2]         == '4')
            return IMAGE_TYPE_MP4;
#endif
         break;

      case 4:
#ifdef HAVE_RJPEG
         if ((ext[0] | 0x20) == 'j' &&
             (ext[1] | 0x20) == 'p' &&
             (ext[2] | 0x20) == 'e' &&
             (ext[3] | 0x20) == 'g')
            return IMAGE_TYPE_JPEG;
#endif
#ifdef HAVE_RWEBP
         if ((ext[0] | 0x20) == 'w' &&
             (ext[1] | 0x20) == 'e' &&
             (ext[2] | 0x20) == 'b' &&
             (ext[3] | 0x20) == 'p')
            return IMAGE_TYPE_WEBP;
#endif
#ifdef HAVE_RWEBM
         if ((ext[0] | 0x20) == 'w' &&
             (ext[1] | 0x20) == 'e' &&
             (ext[2] | 0x20) == 'b' &&
             (ext[3] | 0x20) == 'm')
            return IMAGE_TYPE_WEBM;
#endif
         break;
   }

   return IMAGE_TYPE_NONE;
}

struct image_loader
{
   void *xfer;
   const uint8_t *buf;
   uint32_t *pixels;
   struct texture_compressed *compressed;
   bool (*should_abort)(void *ud);
   void *abort_ud;
   size_t len;
   size_t avail;
   unsigned width;
   unsigned height;
   enum image_type_enum type;
   enum image_loader_state state;
   uint8_t phase;  /* 0: the transfer, 1: the pixels */
   bool pix10;
   bool fp16;
   image_texture_request_t req;
};

bool image_loader_ready(enum image_type_enum type,
      const void *buf, size_t avail)
{
   if (!buf || !avail)
      return false;
   switch (type)
   {
      /* The video stills decode against a growing buffer from the
       * first byte; the picture decoders paint from a prefix once
       * their header is resident. WEBP has no wall to stall at, so
       * it starts only once the still's chunk is whole. */
      case IMAGE_TYPE_WEBM:
      case IMAGE_TYPE_MP4:
         return true;
#ifdef HAVE_RWEBP
      case IMAGE_TYPE_WEBP:
         return rwebp_still_ready(buf, avail);
#endif
#ifdef HAVE_RPNG
      case IMAGE_TYPE_PNG:
         return rpng_header_ready((const uint8_t*)buf, avail);
#endif
#ifdef HAVE_RJPEG
      case IMAGE_TYPE_JPEG:
         return rjpeg_header_ready((const uint8_t*)buf, avail);
#endif
#ifdef HAVE_RTGA
      case IMAGE_TYPE_TGA:
         return rtga_header_ready((const uint8_t*)buf, avail);
#endif
#ifdef HAVE_RBMP
      case IMAGE_TYPE_BMP:
         return rbmp_header_ready((const uint8_t*)buf, avail);
#endif
      default:
         break;
   }
   return false;
}

image_loader_t *image_loader_new(enum image_type_enum type,
      const image_texture_request_t *req)
{
   image_loader_t *l;
   if (type == IMAGE_TYPE_NONE)
      return NULL;
   if (!(l = (image_loader_t*)calloc(1, sizeof(*l))))
      return NULL;
   l->type  = type;
   l->state = IMAGE_LOADER_RUNNING;
   if (req)
      l->req = *req;
   return l;
}

bool image_loader_start(image_loader_t *l, const void *buf, size_t len,
      size_t avail)
{
   if (!l || l->xfer || !buf || !len)
      return false;
   if (!(l->xfer = image_transfer_new(l->type)))
      return false;
   l->buf   = (const uint8_t*)buf;
   l->len   = len;
   l->avail = avail < len ? avail : len;
   /* WEBP has no avail wall: it sees the bytes read so far and no
    * more, and its start gate above admits it only once the still's
    * chunk lies within them. The others wall at the frontier. */
   if (l->type == IMAGE_TYPE_WEBP)
      image_transfer_set_buffer_ptr(l->xfer, l->type,
            (uint8_t*)buf, l->avail);
   else
   {
      image_transfer_set_buffer_ptr(l->xfer, l->type, (uint8_t*)buf, len);
      if (l->avail < len)
         image_transfer_set_avail(l->xfer, l->type, l->avail);
   }
   if (l->req.want_10bit)
      image_transfer_set_want_10bit(l->xfer, l->type, 1);
   if (l->req.want_fp16)
      image_transfer_set_want_fp16(l->xfer, l->type, true);
   /* The channel order now: the JPEG decoder emits final pixels
    * during the transfer, before the process call could name it. */
   image_transfer_set_rgba(l->xfer, l->type, l->req.rgba);
   if (!image_transfer_start(l->xfer, l->type))
   {
      image_transfer_free(l->xfer, l->type);
      l->xfer  = NULL;
      l->state = IMAGE_LOADER_ERROR;
      return false;
   }
   return true;
}

void image_loader_set_avail(image_loader_t *l, size_t avail)
{
   if (!l || !l->xfer)
      return;
   l->avail = avail < l->len ? avail : l->len;
   image_transfer_set_avail(l->xfer, l->type, avail);
}

void image_loader_set_abort(image_loader_t *l,
      bool (*should_abort)(void *ud), void *ud)
{
   if (!l)
      return;
   l->should_abort = should_abort;
   l->abort_ud     = ud;
}

/* The transfer is through: valid, and either the compressed payload
 * the caller asked for is copied out, or the pixels are next. */
static enum image_loader_state image_loader_transferred(image_loader_t *l)
{
   struct image_gpu_layout lay;
   if (!image_transfer_is_valid(l->xfer, l->type))
      return IMAGE_LOADER_ERROR;
   l->phase = 1;
   if (     l->req.want_compressed
         && image_transfer_get_gpu_layout(l->xfer, l->type, l->len, &lay))
   {
      /* The source copied, so the mip pointers outlive the caller's
       * buffer; the CPU decode is deferred to image_texture_realize_rgba */
      struct texture_compressed *tc = (struct texture_compressed*)
         calloc(1, sizeof(*tc));
      if (tc)
      {
         tc->mips    = (struct texture_mip*)
            malloc((size_t)lay.num_mips * sizeof(*tc->mips));
         tc->storage = malloc(l->len);
         if (tc->mips && tc->storage)
         {
            unsigned i;
            memcpy(tc->storage, l->buf, l->len);
            tc->storage_len = l->len;
            tc->num_mips    = lay.num_mips;
            tc->format      = lay.format;
            tc->type        = l->type;
            for (i = 0; i < lay.num_mips; i++)
            {
               tc->mips[i].data   = (const unsigned char*)tc->storage
                                  + lay.offset[i];
               tc->mips[i].width  = lay.width[i];
               tc->mips[i].height = lay.height[i];
               tc->mips[i].size   = lay.size[i];
            }
            l->compressed = tc;
            l->width      = lay.width[0];
            l->height     = lay.height[0];
            return IMAGE_LOADER_DONE;
         }
         free(tc->storage);
         free(tc->mips);
         free(tc);
         /* Out of memory for the copy: the CPU decode below instead */
      }
   }
   return IMAGE_LOADER_RUNNING;
}

enum image_loader_state image_loader_step(image_loader_t *l,
      int64_t (*now)(void), int64_t deadline)
{
   if (!l || !l->xfer)
      return IMAGE_LOADER_ERROR;
   if (l->state == IMAGE_LOADER_DONE || l->state == IMAGE_LOADER_ERROR)
      return l->state;
   l->state = IMAGE_LOADER_RUNNING;

   /* Each loop runs its first iteration whatever the budget, so a
    * caller whose window is spent still progresses; a timed step ends
    * at a phase change rather than beginning the next phase's first
    * pass on top of the time it already took. */
   if (l->phase == 0)
   {
      do
      {
         if (!image_transfer_iterate(l->xfer, l->type))
         {
            /* False both when the transfer is through and when a
             * decoder painting from a prefix stalled at the byte
             * frontier: the next step resumes once more has arrived. */
            if (image_transfer_need_more(l->xfer, l->type))
               return (l->state = IMAGE_LOADER_WAIT);
            if ((l->state = image_loader_transferred(l)) != IMAGE_LOADER_RUNNING
                  || now)
               return l->state;
            break;
         }
         if (l->should_abort && l->should_abort(l->abort_ud))
            return (l->state = IMAGE_LOADER_ERROR);
      } while (!now || now() < deadline);
      if (l->phase == 0)
         return IMAGE_LOADER_RUNNING;
   }

   do
   {
      int ret = image_transfer_process(l->xfer, l->type,
            &l->pixels, l->len, &l->width, &l->height, l->req.rgba);
      switch (ret)
      {
         case IMAGE_PROCESS_NEXT:
            /* Pass by pass (inflate + unfilter), the abort hook between */
            if (l->should_abort && l->should_abort(l->abort_ud))
               return (l->state = IMAGE_LOADER_ERROR);
            break;
         case IMAGE_PROCESS_WAIT:
            /* A video still at the frontier: nothing was consumed */
            return (l->state = IMAGE_LOADER_WAIT);
         case IMAGE_PROCESS_END:
            l->pix10 = image_transfer_is_10bit(l->xfer, l->type);
            l->fp16  = image_transfer_is_fp16(l->xfer, l->type);
            return (l->state = IMAGE_LOADER_DONE);
         default:
            return (l->state = IMAGE_LOADER_ERROR);
      }
   } while (!now || now() < deadline);
   return IMAGE_LOADER_RUNNING;
}

bool image_loader_finish(image_loader_t *l, struct texture_image *img)
{
   if (!l || !img || l->state != IMAGE_LOADER_DONE)
      return false;
   img->pixels        = l->pixels;
   img->compressed    = l->compressed;
   img->width         = l->width;
   img->height        = l->height;
   img->supports_rgba = l->req.rgba;
   img->pix10         = l->pix10;
   img->fp16          = l->fp16;
   l->pixels          = NULL;
   l->compressed      = NULL;
   return true;
}

int image_loader_png_probe(const image_loader_t *l)
{
#ifdef HAVE_RPNG
   int more = 0;
   /* Only a complete read is a verdict: over a partial buffer the walk
    * can only say "not yet", which would read as "still" */
   if (!l || l->type != IMAGE_TYPE_PNG || !l->buf || l->avail < l->len)
      return -1;
   if (rpng_is_apng_ex(l->buf, l->len, &more))
      return 1;
   return 0;
#else
   (void)l;
   return -1;
#endif
}

void *image_loader_detach_anim_stream(image_loader_t *l)
{
   if (!l || !l->xfer)
      return NULL;
   return image_transfer_detach_anim_stream(l->xfer, l->type);
}

void image_loader_free(image_loader_t *l)
{
   if (!l)
      return;
   if (l->xfer)
      image_transfer_free(l->xfer, l->type);
   if (l->compressed)
   {
      free(l->compressed->storage);
      free(l->compressed->mips);
      free(l->compressed);
   }
   free(l->pixels);
   free(l);
}

/* The loader run to completion over a whole buffer: the request from
 * the caller's image, whose ->pix10 asks for 10-bit on the way in. */
static bool image_texture_load_internal(
      enum image_type_enum type,
      void *ptr,
      size_t len,
      struct texture_image *out_img,
      bool (*should_abort)(void *ud), void *abort_ud)
{
   image_texture_request_t req;
   image_loader_t *l;
   enum image_loader_state st;

   req.rgba            = out_img->supports_rgba;
   req.want_10bit      = out_img->pix10;
   req.want_fp16       = false;
   req.want_compressed = true;
   out_img->compressed = NULL;
   if (!(l = image_loader_new(type, &req)))
      return false;
   image_loader_set_abort(l, should_abort, abort_ud);
   if (!image_loader_start(l, ptr, len, len))
   {
      image_loader_free(l);
      return false;
   }
   st = image_loader_step(l, NULL, 0);
   if (st == IMAGE_LOADER_DONE && image_loader_finish(l, out_img))
   {
      image_loader_free(l);
      return true;
   }
   image_loader_free(l);
   return false;
}

bool image_texture_load(struct texture_image *out_img, const char *path)
{
   return image_texture_load_ex(out_img, path, NULL, NULL);
}

void image_texture_free(struct texture_image *img)
{
   if (!img)
      return;

   if (img->compressed)
   {
      free(img->compressed->storage);
      free(img->compressed->mips);
      free(img->compressed);
      img->compressed = NULL;
   }
   if (img->pixels)
      free(img->pixels);
   img->width  = 0;
   img->height = 0;
   img->pixels = NULL;
}

bool image_texture_realize_rgba(struct texture_image *img)
{
   struct texture_compressed *tc;
   void    *decoded = NULL;
   void    *xfer;
   unsigned w       = 0;
   unsigned h       = 0;
   int      ret;

   if (!img)
      return false;
   if (img->pixels)               /* already decoded */
      return true;
   tc = img->compressed;
   if (!tc || !tc->storage)
      return false;

   xfer = image_transfer_new(tc->type);
   if (!xfer)
      return false;
   image_transfer_set_buffer_ptr(xfer, tc->type,
         (uint8_t*)tc->storage, tc->storage_len);
   if (!image_transfer_start(xfer, tc->type))
   {
      image_transfer_free(xfer, tc->type);
      return false;
   }
   while (image_transfer_iterate(xfer, tc->type));
   if (!image_transfer_is_valid(xfer, tc->type))
   {
      image_transfer_free(xfer, tc->type);
      return false;
   }
   do
   {
      ret = image_transfer_process(xfer, tc->type,
            (uint32_t**)&decoded, tc->storage_len, &w, &h,
            img->supports_rgba);
   } while (ret == IMAGE_PROCESS_NEXT);
   image_transfer_free(xfer, tc->type);

   if (     ret == IMAGE_PROCESS_ERROR
         || ret == IMAGE_PROCESS_ERROR_END
         || !decoded)
      return false;

   img->pixels = (uint32_t*)decoded;
   img->width  = w;
   img->height = h;
   return true;
}

void image_texture_narrow_10bit(struct texture_image *img)
{
   size_t n, i;
   uint32_t *px;
   if (!img || !img->pix10 || !img->pixels)
      return;
   px = img->pixels;
   n  = (size_t)img->width * img->height;
   /* Narrow packed XRGB2101010 (R[29:20] G[19:10] B[9:0]) to opaque 8-bit
    * in place, in the order supports_rgba names - so the descriptor is
    * right as it stands and no caller swizzles after. Matches the >> 2
    * narrowing used elsewhere. */
   if (img->supports_rgba)
   {
      uint8_t *d = (uint8_t*)px;
      for (i = 0; i < n; i++, d += 4)
      {
         uint32_t p = px[i];
         d[0] = (uint8_t)(p >> 22);
         d[1] = (uint8_t)(p >> 12);
         d[2] = (uint8_t)(p >>  2);
         d[3] = 0xff;
      }
   }
   else
      for (i = 0; i < n; i++)
      {
         uint32_t p = px[i];
         px[i] = 0xff000000u
               | ((p >>  6) & 0xff0000u)
               | ((p >>  4) & 0x00ff00u)
               | ((p >>  2) & 0x0000ffu);
      }
   img->pix10 = false;
}

/* See image.h. The scratch is four source rows, not the image: the rows a tile
 * band overwrites are copied out before it is written, which is what
 * makes it safe to source from the destination. The source pitch is
 * taken from the unmasked width and the tile width from the masked
 * one, so where the width is not a multiple of four the writes trail
 * the reads rather than running ahead of them. False, with @img
 * untouched, when the scratch cannot be had. */
bool image_texture_tile_gx(struct texture_image *img)
{
   unsigned src_pitch, width2, i;
   size_t   bandsz;
   uint16_t *band;
   uint16_t *dst;

   if (!img || !img->pixels || !img->width || !img->height)
      return false;

   src_pitch = (unsigned)(((size_t)img->width * sizeof(uint32_t)) >> 1);
   bandsz    = (size_t)src_pitch * 4 * sizeof(uint16_t);
   if (!(band = (uint16_t*)malloc(bandsz)))
      return false;

   img->width  &= ~3u;
   img->height &= ~3u;
   width2       = img->width << 1;
   dst          = (uint16_t*)img->pixels;

   for (i = 0; i < img->height; i += 4, dst += 4 * width2)
   {
      const uint16_t *src = band;
      unsigned row;

      memcpy(band, (const uint16_t*)img->pixels + (size_t)i * src_pitch,
            bandsz);

      for (row = 0; row < 4; row++, src += src_pitch)
      {
         unsigned x;
         unsigned off           = row * 4;
         const uint16_t *s      = src;
         uint16_t       *d      = dst;
         for (x = 0; x < width2 >> 3; x++, s += 8, d += 32)
         {
            d[ 0 + off] = s[0];
            d[16 + off] = s[1];
            d[ 1 + off] = s[2];
            d[17 + off] = s[3];
            d[ 2 + off] = s[4];
            d[18 + off] = s[5];
            d[ 3 + off] = s[6];
            d[19 + off] = s[7];
         }
      }
   }

   free(band);
   return true;
}

bool image_texture_load_buffer(struct texture_image *out_img,
   enum image_type_enum type, void *buffer, size_t buffer_len)
{
   if (type != IMAGE_TYPE_NONE)
   {
      if (image_texture_load_internal(
         type, buffer, buffer_len, out_img, NULL, NULL))
         return true;
   }

   out_img->supports_rgba = false;
   out_img->pixels = NULL;
   out_img->width = 0;
   out_img->height = 0;
   out_img->compressed = NULL;

   return false;
}

bool image_texture_load_ex(struct texture_image *out_img,
      const char *path, bool (*should_abort)(void *ud), void *ud)
{
   enum image_type_enum type  = image_texture_get_type(path);

   if (type != IMAGE_TYPE_NONE)
   {
      /* The synchronous read rides the data_transfer prefix spine
       * like every other loader: filestream/VFS routing (overlays
       * and driver assets from archive members or content://
       * documents), 64-bit lengths, the hardware guard behind the
       * read, and honest short-read detection.  A zero budget fills
       * to completion in one blocking call, which is this API's
       * contract. */
      struct data_transfer *dt = data_transfer_open_prefix(path, 0);
      if (dt)
      {
         size_t file_len    = 0;
         const uint8_t *ptr = NULL;

         data_transfer_iterate(dt, 0);
         ptr = data_transfer_ptr(dt, &file_len);
         if (data_transfer_complete(dt) && ptr && file_len
               && image_texture_load_internal(
                     type,
                     (void*)ptr, file_len, out_img, should_abort, ud))
         {
            data_transfer_free(dt);
            return true;
         }
         data_transfer_free(dt);
      }
   }

   out_img->supports_rgba = false;
   out_img->pixels        = NULL;
   out_img->width         = 0;
   out_img->height        = 0;
   out_img->compressed    = NULL;
   out_img->pix10         = false;
   out_img->fp16          = false;

   return false;
}

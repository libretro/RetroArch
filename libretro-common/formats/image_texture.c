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
#include <file/nbio.h>

/* SIMD acceleration for color conversion */
#if defined(__SSE2__)
#include <emmintrin.h>
#define IMAGE_TEXTURE_SIMD_SSE2 1
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#if !defined(VITA) && !defined(WEBOS) && !defined(HAVE_LIBNX)
#include <arm_neon.h>
#define IMAGE_TEXTURE_SIMD_NEON 1
#endif
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
         break;

      case 4:
#ifdef HAVE_RJPEG
         if ((ext[0] | 0x20) == 'j' &&
             (ext[1] | 0x20) == 'p' &&
             (ext[2] | 0x20) == 'e' &&
             (ext[3] | 0x20) == 'g')
            return IMAGE_TYPE_JPEG;
#endif
         break;
   }

   return IMAGE_TYPE_NONE;
}

bool image_texture_color_convert(struct texture_image *out_img)
{
   /* When supports_rgba is set, the decoder produced pixels in ABGR
    * byte order but the renderer expects ARGB.  The only difference
    * is that R and B are swapped.  When supports_rgba is false the
    * pixels are already in ARGB order and no conversion is needed.
    *
    * This replaces the old two-function dance of
    * image_texture_set_color_shifts() + image_texture_color_convert()
    * which computed 4 shift values, passed them through, then either
    * did nothing (non-RGBA) or performed the same R↔B swap (RGBA).
    * Folding eliminates the shift variables, the branch on 4 shift
    * values, and the unreachable generic-shift code path. */
   if (out_img->supports_rgba)
   {
      uint32_t i = 0;
      uint32_t num_pixels = out_img->width * out_img->height;
      uint32_t *pixels    = (uint32_t*)out_img->pixels;

#if defined(IMAGE_TEXTURE_SIMD_SSE2)
      {
         __m128i mask_rb = _mm_set1_epi32(0x00FF00FF);
         __m128i mask_ag = _mm_set1_epi32((int)0xFF00FF00);
         for (; i + 3 < num_pixels; i += 4)
         {
            __m128i px   = _mm_loadu_si128((const __m128i*)(pixels + i));
            __m128i rb   = _mm_and_si128(px, mask_rb);
            __m128i ag   = _mm_and_si128(px, mask_ag);
            __m128i rb_s = _mm_or_si128(_mm_slli_epi32(rb, 16),
                                         _mm_srli_epi32(rb, 16));
            rb_s         = _mm_and_si128(rb_s, mask_rb);
            _mm_storeu_si128((__m128i*)(pixels + i),
                  _mm_or_si128(ag, rb_s));
         }
      }
#elif defined(IMAGE_TEXTURE_SIMD_NEON)
      {
         for (; i + 3 < num_pixels; i += 4)
         {
            uint32x4_t p     = vld1q_u32(pixels + i);
            uint32x4_t rb    = vandq_u32(p, vdupq_n_u32(0x00FF00FF));
            uint32x4_t ag    = vandq_u32(p, vdupq_n_u32(0xFF00FF00));
            uint32x4_t rb_sw = vorrq_u32(vshlq_n_u32(rb, 16),
                                           vshrq_n_u32(rb, 16));
            rb_sw            = vandq_u32(rb_sw, vdupq_n_u32(0x00FF00FF));
            vst1q_u32(pixels + i, vorrq_u32(ag, rb_sw));
         }
      }
#endif
      for (; i < num_pixels; i++)
      {
         uint32_t col = pixels[i];
         uint32_t A   = col & 0xFF000000;
         uint32_t R   = col & 0x00FF0000;
         uint32_t G   = col & 0x0000FF00;
         uint32_t B   = col & 0x000000FF;
         pixels[i]    = A | (B << 16) | G | (R >> 16);
      }
      return true;
   }

   return false;
}

#ifdef GEKKO

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

static bool image_texture_internal_gx_convert_texture32(
      struct texture_image *image)
{
   unsigned tmp_pitch, width2, i;
   const uint16_t *src = NULL;
   uint16_t *dst       = NULL;
   /* Memory allocation in libogc is extremely primitive so try
    * to avoid gaps in memory when converting by copying over to
    * a temporary buffer first, then converting over into
    * main buffer again. */
   void *tmp           = malloc(image->width
         * image->height * sizeof(uint32_t));

   if (!tmp)
      return false;

   memcpy(tmp, image->pixels, image->width
         * image->height * sizeof(uint32_t));
   tmp_pitch = (image->width * sizeof(uint32_t)) >> 1;

   image->width       &= ~3;
   image->height      &= ~3;
   width2              = image->width << 1;
   src                 = (uint16_t*)tmp;
   dst                 = (uint16_t*)image->pixels;

   for (i = 0; i < image->height; i += 4, dst += 4 * width2)
   {
      GX_BLIT_LINE_32(0)
      GX_BLIT_LINE_32(4)
      GX_BLIT_LINE_32(8)
      GX_BLIT_LINE_32(12)
   }

   free(tmp);
   return true;
}
#endif

static bool image_texture_load_internal(
      enum image_type_enum type,
      void *ptr,
      size_t len,
      struct texture_image *out_img)
{
   int ret;
   void *img    = image_transfer_new(type);

   if (!img)
      return false;

   image_transfer_set_buffer_ptr(img, type, (uint8_t*)ptr, len);

   if (!image_transfer_start(img, type))
   {
      image_transfer_free(img, type);
      return false;
   }

   while (image_transfer_iterate(img, type));

   if (!image_transfer_is_valid(img, type))
   {
      image_transfer_free(img, type);
      return false;
   }

   do
   {
      ret = image_transfer_process(img, type,
            (uint32_t**)&out_img->pixels, len, &out_img->width,
            &out_img->height);
   } while (ret == IMAGE_PROCESS_NEXT);

   if (ret == IMAGE_PROCESS_ERROR || ret == IMAGE_PROCESS_ERROR_END)
   {
      image_transfer_free(img, type);
      return false;
   }

   image_texture_color_convert(out_img);

#ifdef GEKKO
   if (!image_texture_internal_gx_convert_texture32(out_img))
   {
      image_texture_free(out_img);
      image_transfer_free(img, type);
      return false;
   }
#endif

   image_transfer_free(img, type);
   return true;
}

void image_texture_free(struct texture_image *img)
{
   if (!img)
      return;

   if (img->pixels)
      free(img->pixels);
   img->width  = 0;
   img->height = 0;
   img->pixels = NULL;
}

bool image_texture_load_buffer(struct texture_image *out_img,
   enum image_type_enum type, void *buffer, size_t buffer_len)
{
   if (type != IMAGE_TYPE_NONE)
   {
      if (image_texture_load_internal(
         type, buffer, buffer_len, out_img))
         return true;
   }

   out_img->supports_rgba = false;
   out_img->pixels = NULL;
   out_img->width = 0;
   out_img->height = 0;

   return false;
}

bool image_texture_load(struct texture_image *out_img,
      const char *path)
{
   enum image_type_enum type  = image_texture_get_type(path);

   if (type != IMAGE_TYPE_NONE)
   {
      struct nbio_t *handle = (struct nbio_t*)
         nbio_open(path, NBIO_READ);
      if (handle)
      {
         void *ptr       = NULL;
         size_t file_len = 0;

         /* Fast path: collapses begin_read + iterate-loop + get_ptr
          * into a single call. For mmap this is zero-copy (instant),
          * for AIO a single blocking syscall, for stdio one fread. */
         if ((ptr = nbio_load_entire(handle, &file_len)))
         {
            if (image_texture_load_internal(
                     type,
                     ptr, file_len, out_img))
            {
               nbio_free(handle);
               return true;
            }
         }
         nbio_free(handle);
      }
   }

   out_img->supports_rgba = false;
   out_img->pixels        = NULL;
   out_img->width         = 0;
   out_img->height        = 0;

   return false;
}

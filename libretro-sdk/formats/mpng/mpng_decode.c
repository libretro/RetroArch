#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <boolean.h>

#ifdef WANT_MINIZ
#define MINIZ_HEADER_FILE_ONLY
#include "miniz.c"
#endif

#include <formats/mpng.h>

static uint32_t dword_be(const uint8_t *buf)
{
   return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3] << 0);
}

static uint16_t word_be(const uint8_t *buf)
{
   return (buf[0] << 16) | (buf[1] << 8) | (buf[2] << 0);
}

#define read8r(source) (*(source))
#define read8(target, chunkdata) do { target = read8r(chunkdata); chunkdata += 1; } while(0)
#define read24(target, chunkdata) do { target = word_be(chunkdata); chunkdata  += 3; } while(0)
#define read32(target, chunkdata) do { target = dword_be(chunkdata); chunkdata += 4; } while(0)

enum mpng_chunk_type
{
   MPNG_CHUNK_TRNS = 0x74524E53,
   MPNG_CHUNK_IHDR = 0x49484452,
   MPNG_CHUNK_IDAT = 0x49444154,
   MPNG_CHUNK_PLTE = 0x504c5445,
   MPNG_CHUNK_IEND = 0x49454e44,
};

struct mpng_ihdr
{
   uint32_t width;
   uint32_t height;
   uint8_t depth;
   uint8_t color_type;
   uint8_t compression;
   uint8_t filter;
   uint8_t interlace;
};

bool mpng_parse_ihdr(struct mpng_ihdr *ihdr, const uint8_t * chunk,
      enum video_format format, unsigned int *bpl,
      uint8_t *pixels, uint8_t *pixelsat, uint8_t *pixelsend)
{
   read32(ihdr->width, chunk);
   read32(ihdr->height, chunk);
   read8(ihdr->depth, chunk);
   read8(ihdr->color_type, chunk);
   read8(ihdr->compression, chunk);
   read8(ihdr->filter, chunk);
   read8(ihdr->interlace, chunk);

   if (ihdr->width >= 0x80000000)
      return false;

   if (ihdr->width == 0 || ihdr->height == 0)
      return false;

   if (ihdr->height >= 0x80000000)
      return false;

   if (     ihdr->color_type != 2
         && ihdr->color_type != 3
         && ihdr->color_type != 6)
      return false;


   if (ihdr->compression != 0)
      return false;
   if (ihdr->filter != 0)
      return false;
   if (ihdr->interlace != 0 && ihdr->interlace != 1)
      return false;

   /*
    * Greyscale 	0
    * Truecolour 	2
    * Indexed-colour 	3
    * Greyscale with alpha 	4
    * Truecolour with alpha 	6
    **/

   switch (ihdr->color_type)
   {
      case 2:
         /* Truecolor; can be 16bpp but I don't want that. */
         if (ihdr->depth != 8)
            return false;
         *bpl = 3 * ihdr->width;
         break;
      case 3:
         /* Paletted. */
         if (ihdr->depth != 1
               && ihdr->depth != 2
               && ihdr->depth != 4
               && ihdr->depth != 8)
            return false;
         *bpl = (ihdr->width * ihdr->depth + ihdr->depth - 1) / 8;
         break;
      case 6:
         /* Truecolor with alpha. */
         if (ihdr->depth != 8)
            return false;

         /* Can only decode alpha on ARGB formats. */
         if (format != FMT_ARGB8888)
            return false;
         *bpl = 4 * ihdr->width;
         break;
   }

   pixels    = (uint8_t*)malloc((*bpl + 1) * ihdr->height);

   if (!pixels)
      return false;

   pixelsat  = pixels;
   pixelsend = pixels + (*bpl + 1) * ihdr->height;

   return true;
}

bool png_decode(const void *userdata, size_t len,
      struct mpng_image *img, enum video_format format)
{
   struct mpng_ihdr ihdr = {0};
   unsigned int bpl;
   unsigned int palette[256];
   int palette_len = 0;
   uint8_t *pixelsat = NULL;
   uint8_t *pixelsend = NULL;
   uint8_t *pixels = NULL;
   const uint8_t *data_end = NULL;
   const uint8_t *data = (const uint8_t*)userdata;

   if (!data)
      return false;

   memset(img, 0, sizeof(struct mpng_image));

   /* Only works for RGB888, XRGB8888, and ARGB8888 */
   if      (format != FMT_RGB888 
         && format != FMT_XRGB8888
         && format != FMT_ARGB8888)
      return false;

   if (len < 8)
      return false;

   if (memcmp(data, "\x89PNG\r\n\x1A\n", 8))
      return false;

   data_end  = data + len;
   data     += 8;

   memset(palette, 0, sizeof(palette));

#ifdef WANT_MINIZ
   tinfl_decompressor inflator;
   tinfl_init(&inflator);
#endif

   while (1)
   {
      unsigned int chunklen, chunktype;
      unsigned int chunkchecksum;
      unsigned int actualchunkchecksum;
      const uint8_t * chunkdata = NULL;

      if ((data + 4 + 4) > data_end)
         goto error;

      chunklen  = dword_be(data);
      chunktype = dword_be(data+4);

      if (chunklen>=0x80000000)
         goto error;
      if ((data + 4 + chunklen + 4) > data_end)
         goto error;

      chunkchecksum       = mz_crc32(mz_crc32(0, NULL, 0), (uint8_t*)data+4, 4 + chunklen);
      chunkdata           = data + 4 + 4;
      actualchunkchecksum = dword_be(data + 4 + 4 + chunklen);

      if (actualchunkchecksum != chunkchecksum)
         goto error;

      data += 4 + 4 + chunklen + 4;

      switch (chunktype)
      {
         case MPNG_CHUNK_IHDR:
            if (!mpng_parse_ihdr(&ihdr, chunkdata, format, &bpl, pixels,
                     pixelsat, pixelsend))
               goto error;
            break;
         case MPNG_CHUNK_PLTE:
            {
               unsigned i;

               if (!pixels || palette_len != 0)
                  goto error;
               if (chunklen == 0 || chunklen % 3 || chunklen > 3 * 256)
                  goto error;

               /* Palette on RGB is allowed but rare, 
                * and it's just a recommendation anyways. */
               if (ihdr.color_type != 3)
                  break;

               palette_len = chunklen / 3;

               for (i = 0; i < palette_len; i++)
               {
                  read24(palette[i], chunkdata);
                  palette[i] |= 0xFF000000;
               }
            }
            break;
         case MPNG_CHUNK_TRNS:
            {
               if (format != FMT_ARGB8888 || !pixels || pixels != pixelsat)
                  goto error;

               if (ihdr.color_type == 2)
               {
                  if (palette_len == 0)
                     goto error;
                  goto error;
               }
               else if (ihdr.color_type == 3)
                  goto error;
               else
                  goto error;
            }
            break;
         case MPNG_CHUNK_IDAT:
            {
               size_t byteshere;
               size_t chunklen_copy;
#ifdef WANT_MINIZ
               tinfl_status status;
#endif

               if (!pixels || (ihdr.color_type == 3 && palette_len == 0))
                  goto error;

               chunklen_copy       = chunklen;
               byteshere           = (pixelsend - pixelsat)+1;

#ifdef WANT_MINIZ
               status = tinfl_decompress(&inflator,
                     (const mz_uint8 *)chunkdata,
                     &chunklen_copy, pixels,
                     pixelsat,
                     &byteshere,
                     TINFL_FLAG_HAS_MORE_INPUT | 
                     TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF | 
                     TINFL_FLAG_PARSE_ZLIB_HEADER);
#endif

               pixelsat += byteshere;

#ifdef WANT_MINIZ
               if (status < TINFL_STATUS_DONE)
                  goto error;
#endif
            }
            break;
         case MPNG_CHUNK_IEND:
            {
               unsigned b, x, y;
#ifdef WANT_MINIZ
               tinfl_status status;
#endif
               size_t finalbytes;
               unsigned int bpp_packed;
               uint8_t *prevout;
               size_t            zero = 0;
               uint8_t *          out = NULL;
               uint8_t * filteredline = NULL;

               if (data != data_end)
                  goto error;
               if (chunklen)
                  goto error;

               finalbytes = (pixelsend - pixelsat);

#ifdef WANT_MINIZ
               status = tinfl_decompress(&inflator, (const mz_uint8 *)NULL, &zero, pixels, pixelsat, &finalbytes,
                     TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF | TINFL_FLAG_PARSE_ZLIB_HEADER);
#endif

               pixelsat += finalbytes;

#ifdef WANT_MINIZ
               if (status < TINFL_STATUS_DONE)
                  goto error;

               if (status > TINFL_STATUS_DONE)
                  goto error;
#endif
               /* Too little data (can't be too much 
                * because we didn't give it that buffer size) */
               if (pixelsat != pixelsend)
                  goto error; 

               out = (uint8_t*)malloc(videofmt_byte_per_pixel(format) * ihdr.width * ihdr.height);

               /* TODO: deinterlace at random point */

               /* run filters */
               bpp_packed = ((ihdr.color_type == 2) ? 3 : (ihdr.color_type == 6) ? 4 : 1);
               prevout    = (out + (4 * ihdr.width * 1));

               /* This will blow up if a 1px high image 
                * is filtered with Paeth, but highly unlikely. */
               if (ihdr.height==1)
                  prevout = out;

               /* Not using bpp here because we only need a chunk of black anyways */
               memset(prevout, 0, 4 * ihdr.width * 1);

               filteredline = pixels;

               for (y = 0; y < ihdr.height; y++)
               {
                  uint8_t *thisout = (uint8_t*)(out + (bpl*y));

                  switch (*(filteredline++))
                  {
                     case 0:
                        memcpy(thisout, filteredline, bpl);
                        break;
                     case 1:
                        memcpy(thisout, filteredline, bpp_packed);

                        for (x = bpp_packed; x < bpl; x++)
                           thisout[x] = thisout[x - bpp_packed] + filteredline[x];
                        break;
                     case 2:
                        for (x = 0; x < bpl; x++)
                           thisout[x] = prevout[x] + filteredline[x];
                        break;
                     case 3:
                        for (x = 0; x < bpp_packed; x++)
                        {
                           int a      = 0;
                           int b      = prevout[x];
                           thisout[x] = (a+b)/2 + filteredline[x];
                        }
                        for (x = bpp_packed; x < bpl; x++)
                        {
                           int a      = thisout[x - bpp_packed];
                           int b      = prevout[x];
                           thisout[x] = (a + b) / 2 + filteredline[x];
                        }
                        break;
                     case 4:
                        for (x = 0; x < bpp_packed; x++)
                        {
                           int prediction;

                           int a   = 0;
                           int b   = prevout[x];
                           int c   = 0;

                           int p   = a+b-c;
                           int pa  = abs(p-a);
                           int pb  = abs(p-b);
                           int pc  = abs(p-c);

                           if (pa <= pb && pa <= pc)
                              prediction=a;
                           else if (pb <= pc)
                              prediction=b;
                           else
                              prediction=c;

                           thisout[x] = filteredline[x]+prediction;
                        }

                        for (x = bpp_packed; x < bpl; x++)
                        {
                           int prediction;

                           int a   = thisout[x - bpp_packed];
                           int b   = prevout[x];
                           int c   = prevout[x - bpp_packed];

                           int p   = a+b-c;
                           int pa  = abs(p-a);
                           int pb  = abs(p-b);
                           int pc  = abs(p-c);

                           if (pa <= pb && pa <= pc)
                              prediction = a;
                           else if (pb <= pc)
                              prediction = b;
                           else
                              prediction = c;

                           thisout[x] = filteredline[x] + prediction;
                        }
                        break;
                     default:
                        goto error;
                  }
                  prevout       = thisout;
                  filteredline += bpl;
               }

               /* Unpack paletted data
                * not sure if these aliasing tricks are valid,
                * but the prerequisites for that bugging up 
                * are pretty much impossible to hit.
                **/
               if (ihdr.color_type == 3)
               {
                  switch (ihdr.depth)
                  {
                     case 1:
                        {
                           int y          = ihdr.height;
                           uint8_t * outp = out + 3 * ihdr.width * ihdr.height;
                           do
                           {
                              uint8_t * inp = (uint8_t*)(out + y * bpl);
                              int x         = (ihdr.width + 7) / 8;
                              do
                              {
                                 x--;
                                 inp--;
                                 for (b = 0; b < 8; b++)
                                 {
                                    int rgb32 = palette[((*inp)>>b)&1];
                                    *(--outp) = rgb32 >> 0;
                                    *(--outp) = rgb32 >> 8;
                                    *(--outp) = rgb32 >> 16;
                                 }
                              } while(x);
                              y--;
                           } while(y);
                        }
                        break;
                     case 2:
                        {
                           int y          = ihdr.height;
                           uint8_t * outp = (uint8_t*)(out + 3 * ihdr.width * ihdr.height);
                           do
                           {
                              unsigned char *inp = out + y * bpl;
                              int x              = (ihdr.width + 3) / 4;
                              do
                              {
                                 int b;
                                 x--;
                                 inp--;
                                 for (b = 0;b < 8; b += 2)
                                 {
                                    int rgb32 = palette[((*inp)>>b)&3];
                                    *(--outp) = rgb32 >> 0;
                                    *(--outp) = rgb32 >> 8;
                                    *(--outp) = rgb32 >> 16;
                                 }
                              } while(x);
                              y--;
                           } while(y);
                        }
                        break;
                     case 4:
                        {
                           int y         = ihdr.height;
                           uint8_t *outp = out + 3 * ihdr.width * ihdr.height;

                           do
                           {
                              unsigned char *inp = out + y * bpl;
                              int x              = (ihdr.width + 1) / 2;

                              do
                              {
                                 int rgb32;

                                 x--;
                                 inp--;
                                 rgb32     = palette[*inp&15];
                                 *(--outp) = rgb32 >> 0;
                                 *(--outp) = rgb32 >> 8;
                                 *(--outp) = rgb32 >> 16;
                                 rgb32     = palette[*inp>>4];
                                 *(--outp) = rgb32 >> 0;
                                 *(--outp) = rgb32 >> 8;
                                 *(--outp) = rgb32 >> 16;
                              } while(x);
                              y--;
                           } while(y);
                        }
                        break;
                     case 8:
                        {
                           uint8_t *inp  = (uint8_t*)(out + ihdr.width * ihdr.height);
                           uint8_t *outp = (uint8_t*)(out + 3 * ihdr.width * ihdr.height);
                           int i         = ihdr.width * ihdr.height;
                           do
                           {
                              int rgb32;
                              i--;
                              inp       -= 1;
                              rgb32      = palette[*inp];

                              *(--outp)  = rgb32 >> 0;
                              *(--outp)  = rgb32 >> 8;
                              *(--outp)  = rgb32 >> 16;
                           } while(i);
                        }
                        break;
                  }
               }

               /* unpack to 32bpp if requested */
               if (format != FMT_RGB888 && ihdr.color_type == 2)
               {
                  uint8_t  *inp   = (uint8_t*)(out + ihdr.width * ihdr.height * 3);
                  uint32_t *outp  = (uint32_t*)(((uint32_t*)out) + ihdr.width * ihdr.height);
                  int i           = ihdr.width * ihdr.height;

                  do
                  {
                     i--;
                     inp-=3;
                     outp--;
                     *outp = word_be(inp) | 0xFF000000;
                  } while(i);
               }

               img->width    = ihdr.width;
               img->height   = ihdr.height;
               img->pixels   = out;
               img->pitch    = videofmt_byte_per_pixel(format) * ihdr.width;
               img->format   = format;
               free(pixels);
               return true;
            }
            break;
         default:
            if (!(chunktype & 0x20000000))
               goto error;//unknown critical
            //otherwise ignore
      }
   }

error:
   free(pixels);
   memset(img, 0, sizeof(struct mpng_image));
   return false;
}

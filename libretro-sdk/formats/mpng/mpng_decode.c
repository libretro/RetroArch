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
#define read8(target) do { target = read8r(chunkdata); chunkdata += 1; } while(0)
#define read24(target) do { target = word_be(chunkdata); chunkdata  += 3; } while(0)
#define read32(target) do { target = dword_be(chunkdata); chunkdata += 4; } while(0)

enum mpng_chunk_type
{
   MPNG_CHUNK_TRNS = 0x74524E53,
   MPNG_CHUNK_IHDR = 0x49484452,
   MPNG_CHUNK_IDAT = 0x49444154,
   MPNG_CHUNK_PLTE = 0x504c5445,
   MPNG_CHUNK_IEND = 0x49454e44,
};

bool png_decode(const void *userdata, size_t len,
      struct mpng_image *img, enum video_format format)
{
   /* chop off some warnings... these are all initialized in IHDR */
   unsigned int depth;
   unsigned int color_type;
   unsigned int compression;
   unsigned int filter;
   unsigned int interlace;
   unsigned int bpl;
   unsigned int palette[256];
   unsigned int width;
   unsigned int height;
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
            {
               read32(width);
               read32(height);
               read8(depth);
               read8(color_type);
               read8(compression);
               read8(filter);
               read8(interlace);

               if (width >= 0x80000000)
                  goto error;

               if (width == 0 || height == 0)
                  goto error;

               if (height >= 0x80000000)
                  goto error;

               if (color_type != 2 && color_type != 3 && color_type != 6)
                  goto error;

               /*
                * Greyscale 	0
                * Truecolour 	2
                * Indexed-colour 	3
                * Greyscale with alpha 	4
                * Truecolour with alpha 	6
                **/
               if (color_type == 2 && depth != 8)
                  goto error; /* truecolor; can be 16bpp but I don't want that. */

               if (color_type == 3 && depth != 1 && depth != 2 && depth != 4 && depth != 8)
                  goto error; /* paletted */

               if (color_type == 6 && depth != 8)
                  goto error; /* truecolor with alpha */

               if (color_type == 6 && format != FMT_ARGB8888)
                  goto error; /* can only decode alpha on ARGB formats */

               if (compression != 0)
                  goto error;

               if (filter != 0)
                  goto error;
               if (interlace != 0 && interlace != 1)
                  goto error;

               if (color_type == 2)
                  bpl = 3 * width;
               if (color_type == 3)
                  bpl = (width * depth + depth - 1) / 8;
               if (color_type == 6)
                  bpl = 4 * width;

               pixels = (uint8_t*)malloc((bpl + 1) * height);

               if (!pixels)
                  goto error;

               pixelsat  = pixels;
               pixelsend = pixels + (bpl + 1) * height;
            }
            break;
         case MPNG_CHUNK_PLTE:
            {
               unsigned i;

               if (!pixels || palette_len != 0)
                  goto error;
               if (chunklen == 0 || chunklen % 3 || chunklen > 3 * 256)
                  goto error;

               /* palette on RGB is allowed but rare, 
                * and it's just a recommendation anyways. */
               if (color_type != 3)
                  break;

               palette_len = chunklen / 3;

               for (i = 0; i < palette_len; i++)
               {
                  read24(palette[i]);
                  palette[i] |= 0xFF000000;
               }
            }
            break;
         case MPNG_CHUNK_TRNS:
            {
               if (format != FMT_ARGB8888 || !pixels || pixels != pixelsat)
                  goto error;

               if (color_type == 2)
               {
                  if (palette_len == 0)
                     goto error;
                  goto error;
               }
               else if (color_type == 3)
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

               if (!pixels || (color_type == 3 && palette_len == 0))
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

               if (pixelsat != pixelsend)
                  goto error; /* too little data (can't be too much because we didn't give it that buffer size) */

               out = (uint8_t*)malloc(videofmt_byte_per_pixel(format) * width * height);

               //TODO: deinterlace at random point

               //run filters
               unsigned int bpppacked = ((color_type == 2) ? 3 : (color_type == 6) ? 4 : 1);
               uint8_t * prevout      = out + (4 * width * 1);

               if (height==1)
               {
                  /* This will blow up if a 1px high image is filtered with Paeth, but highly unlikely. */
                  prevout=out;
               }

               /* Not using bpp here because we only need a chunk of black anyways */
               memset(prevout, 0, 4 * width * 1);

               filteredline = pixels;

               for (y = 0; y < height; y++)
               {
                  uint8_t * thisout=out+(bpl*y);

                  switch (*(filteredline++))
                  {
                     case 0:
                        memcpy(thisout, filteredline, bpl);
                        break;
                     case 1:
                        memcpy(thisout, filteredline, bpppacked);

                        for (x = bpppacked; x < bpl; x++)
                           thisout[x] = thisout[x-bpppacked] + filteredline[x];
                        break;
                     case 2:
                        for (x = 0; x < bpl; x++)
                           thisout[x] = prevout[x] + filteredline[x];
                        break;
                     case 3:
                        for (x = 0; x < bpppacked; x++)
                        {
                           int a      = 0;
                           int b      = prevout[x];
                           thisout[x] = (a+b)/2 + filteredline[x];
                        }
                        for (x = bpppacked; x < bpl; x++)
                        {
                           int a      = thisout[x - bpppacked];
                           int b      = prevout[x];
                           thisout[x] = (a + b) / 2 + filteredline[x];
                        }
                        break;
                     case 4:
                        for (x = 0; x < bpppacked; x++)
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

                        for (x = bpppacked; x < bpl; x++)
                        {
                           int prediction;

                           int a   = thisout[x-bpppacked];
                           int b   = prevout[x];
                           int c   = prevout[x-bpppacked];

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
               if (color_type == 3)
               {
                  switch (depth)
                  {
                     case 1:
                        {
                           int y=height;
                           uint8_t * outp=out+3*width*height;
                           do
                           {
                              uint8_t * inp=out+y*bpl;

                              int x=(width+7)/8;
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
                           int y=height;
                           uint8_t * outp=out+3*width*height;
                           do
                           {
                              unsigned char *inp = out + y * bpl;
                              int x              = (width + 3) / 4;
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
                           int y = height;
                           uint8_t *outp = out + 3 * width * height;

                           do
                           {
                              unsigned char *inp = out + y * bpl;
                              int x              = (width+1) / 2;

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
                           uint8_t *inp  = out + width * height;
                           uint8_t *outp = out + 3 * width * height;
                           int i         = width * height;
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
               if (format != FMT_RGB888 && color_type == 2)
               {
                  uint8_t  *inp   = out + width * height * 3;
                  uint32_t *outp  = ((uint32_t*)out) + width * height;
                  int i           = width*height;

                  do
                  {
                     i--;
                     inp-=3;
                     outp--;
                     *outp = word_be(inp) | 0xFF000000;
                  } while(i);
               }

               img->width    = width;
               img->height   = height;
               img->pixels   = out;
               img->pitch    = videofmt_byte_per_pixel(format)*width;
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

/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rvp9.h>
#ifdef HAVE_THREADS
#include <rthreads/tpool.h>
#endif

static unsigned read32(const uint8_t *p)
{
   return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16)
          | ((unsigned)p[3] << 24);
}

static int write_plane(FILE *out, const uint8_t *data, unsigned width,
      unsigned height, unsigned stride, unsigned bytes)
{
   unsigned y, x;
   uint16_t endian = 1;
   for (y = 0; y < height; y++)
   {
      const uint8_t *row = data + (size_t)y * stride * bytes;
      if (bytes == 1 || *(const uint8_t *)&endian)
      {
         if (fwrite(row, bytes, width, out) != width)
            return 0;
      }
      else
      {
         const uint16_t *pixels = (const uint16_t *)row;
         for (x = 0; x < width; x++)
            if (fputc(pixels[x] & 255, out) == EOF
                  || fputc(pixels[x] >> 8, out) == EOF)
               return 0;
      }
   }
   return 1;
}

int main(int argc, char **argv)
{
   FILE *input = NULL, *out = NULL;
   rvp9_dec *d = NULL;
   uint8_t header[32], *data = NULL;
   unsigned workers, count   = 0, bytes, size;
   size_t got;
   int ret = 1, show;
#ifdef HAVE_THREADS
   tpool_t *pool = NULL;
#endif
   if (argc != 4)
      goto done;
   workers = (unsigned)strtoul(argv[3], NULL, 10);
   if (!workers || workers > 4)
      goto done;
   input = fopen(argv[1], "rb");
   out   = fopen(argv[2], "wb");
   d     = (rvp9_dec *)calloc(1, sizeof(*d));
   if (!input || !out || !d || fread(header, 1, 32, input) != 32
         || memcmp(header, "DKIF", 4) || memcmp(header + 8, "VP90", 4))
      goto done;
   memset(d->iht_scratch, 0xa5, sizeof(d->iht_scratch));
#ifdef HAVE_THREADS
   if (workers > 1)
   {
      pool = tpool_create(workers - 1);
      if (!pool)
         goto done;
      rvp9_set_tile_pool(d, pool, workers);
   }
#else
   if (workers != 1)
      goto done;
#endif
   while ((got = fread(header, 1, 12, input)) != 0)
   {
      rvp9_fb *fb;
      size = read32(header);
      if (got != 12 || !size || size > 16u * 1024u * 1024u)
         goto done;
      data = (uint8_t *)malloc(size);
      if (!data || fread(data, 1, size, input) != size
            || rvp9_decode_frame(d, data, size, &show) < 0)
         goto done;
      free(data);
      data = NULL;
      if (show < 0)
         continue;
      if ((unsigned)show >= sizeof(d->fbs) / sizeof(d->fbs[0]))
         goto done;
      fb    = &d->fbs[show];
      bytes = d->fb_bit_depth == 8 ? 1 : 2;
      if (!write_plane(out, fb->y, fb->w, fb->h, d->ys, bytes)
            || !write_plane(
                  out, fb->u, (fb->w + 1) / 2, (fb->h + 1) / 2, d->uvs, bytes)
            || !write_plane(
                  out, fb->v, (fb->w + 1) / 2, (fb->h + 1) / 2, d->uvs, bytes))
         goto done;
      count++;
   }
   if (!feof(input) || !count || fflush(out))
      goto done;
   printf("VP9 pixels: %u frames, %u workers, decoder %lu bytes\n", count,
         workers, (unsigned long)sizeof(*d));
   ret = 0;
done:
   if (ret)
      fprintf(stderr, "VP9 pixel decode failed at frame %u\n", count);
   free(data);
   if (d)
   {
      rvp9_free(d);
      free(d);
   }
#ifdef HAVE_THREADS
   if (pool)
      tpool_destroy(pool);
#endif
   if (input)
      fclose(input);
   if (out && fclose(out))
      ret = 1;
   return ret;
}

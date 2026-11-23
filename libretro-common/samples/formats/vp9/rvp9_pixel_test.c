/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rvp9.h>
#include <formats/rvp8.h>
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

struct retained_frame
{
   struct retained_frame *next;
   rvp9_fb fb;
   void *owner;
   void (*release)(void*);
   unsigned ys, uvs, bytes;
   uint32_t hash;
};

static uint32_t retained_hash(const struct retained_frame *lease)
{
   uint32_t hash = 2166136261u;
   unsigned plane, x, y;
   for (plane = 0; plane < 3; plane++)
   {
      const uint8_t *p = plane == 0 ? lease->fb.y
            : plane == 1 ? lease->fb.u : lease->fb.v;
      unsigned width = plane ? (lease->fb.w + 1) / 2 : lease->fb.w;
      unsigned height = plane ? (lease->fb.h + 1) / 2 : lease->fb.h;
      unsigned stride = plane ? lease->uvs : lease->ys;
      for (y = 0; y < height; y++)
         for (x = 0; x < width * lease->bytes; x++)
            hash = (hash ^ p[(size_t)y * stride * lease->bytes + x])
                  * 16777619u;
   }
   return hash;
}

int main(int argc, char **argv)
{
   FILE *input = NULL, *out = NULL;
   rvp9_dec *d = NULL;
   rvp8_video *vp8 = NULL;
   int is_vp8 = 0;
   struct retained_frame *held = NULL;
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
         || memcmp(header, "DKIF", 4)
         || (memcmp(header + 8, "VP90", 4) && memcmp(header + 8, "VP80", 4)))
      goto done;
   is_vp8 = !memcmp(header + 8, "VP80", 4);
   if (is_vp8 && (workers != 1 || !(vp8 = rvp8_video_open())))
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
      rvp9_fb *fb, vp8_view;
      int ys, uvs;
      size = read32(header);
      if (got != 12 || !size || size > 16u * 1024u * 1024u)
         goto done;
      data = (uint8_t *)malloc(size);
      if (!data || fread(data, 1, size, input) != size)
         goto done;
      if (is_vp8)
      {
         if (rvp8_video_decode(vp8, data, size) != 0)
            goto done;
         show = 0;
         if (!count)
         {
            unsigned w, h;
            uint32_t *pixels = rvp8_decode(data, size, &w, &h, 0);
            if (!pixels)
               goto done;
            free(pixels);
         }
      }
      else if (rvp9_decode_frame(d, data, size, &show) < 0)
         goto done;
      free(data);
      data = NULL;
      if (show < 0)
         continue;
      if ((unsigned)show >= sizeof(d->fbs) / sizeof(d->fbs[0]))
         goto done;
      if (is_vp8)
      {
         memset(&vp8_view, 0, sizeof(vp8_view));
         vp8_view.y = (uint8_t*)rvp8_video_plane(vp8, 0, &ys,
               &vp8_view.w, &vp8_view.h);
         vp8_view.u = (uint8_t*)rvp8_video_plane(vp8, 1, &uvs, NULL, NULL);
         vp8_view.v = (uint8_t*)rvp8_video_plane(vp8, 2, NULL, NULL, NULL);
         fb = &vp8_view;
         bytes = 1;
      }
      else
      {
         fb = &d->fbs[show];
         bytes = d->fb_bit_depth == 8 ? 1 : 2;
         ys = d->ys;
         uvs = d->uvs;
      }
      if (!write_plane(out, fb->y, fb->w, fb->h, ys, bytes)
            || !write_plane(
                  out, fb->u, (fb->w + 1) / 2, (fb->h + 1) / 2, uvs, bytes)
            || !write_plane(
                  out, fb->v, (fb->w + 1) / 2, (fb->h + 1) / 2, uvs, bytes))
         goto done;
      {
         struct retained_frame *lease =
               (struct retained_frame*)malloc(sizeof(*lease));
         if (!lease)
            goto done;
         lease->owner = is_vp8 ? rvp8_video_retain_planes(vp8)
               : rvp9_retain_planes(d, fb->y);
         lease->release = is_vp8 ? rvp8_video_release_planes : rvp9_release_planes;
         if (!lease->owner)
         {
            free(lease);
            goto done;
         }
         lease->fb = *fb;
         lease->ys = ys;
         lease->uvs = uvs;
         lease->bytes = bytes;
         lease->hash = retained_hash(lease);
         lease->next = held;
         held = lease;
      }
      count++;
   }
   if (!feof(input) || !count || fflush(out))
      goto done;
   printf("VP8/VP9 pixels: %u frames, %u workers, decoder %lu bytes\n", count,
         workers, (unsigned long)sizeof(*d));
   ret = 0;
done:
   if (ret)
      fprintf(stderr, "VP8/VP9 pixel decode failed at frame %u\n", count);
   free(data);
   rvp8_video_close(vp8);
   if (d)
   {
      rvp9_free(d);
      free(d);
   }
#ifdef HAVE_THREADS
   if (pool)
      tpool_destroy(pool);
#endif
   while (held)
   {
      struct retained_frame *next = held->next;
      if (retained_hash(held) != held->hash)
      {
         fprintf(stderr, "Retained video planes changed after decoding/free\n");
         ret = 1;
      }
      held->release(held->owner);
      free(held);
      held = next;
   }
   if (input)
      fclose(input);
   if (out && fclose(out))
      ret = 1;
   return ret;
}

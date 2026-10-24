/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rvp8.h>

static size_t copied;
static unsigned fail_malloc, allocations;
void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size)
{
   if (fail_malloc && ++allocations == fail_malloc) return NULL;
   return __real_malloc(size);
}
void *__real_memcpy(void *dst, const void *src, size_t size);
void *__wrap_memcpy(void *dst, const void *src, size_t size)
{
   if (size > 4096) copied += size;
   return __real_memcpy(dst, src, size);
}

static unsigned little32(const uint8_t *p)
{
   return p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16)
      | ((unsigned)p[3] << 24);
}

static int allocation_failures(FILE *input, uint8_t *packet, size_t capacity)
{
   unsigned failure;
   for (failure = 1; failure <= 4; failure++)
   {
      rvp8_video *decoder = rvp8_video_open();
      uint8_t header[12];
      int ret = 0;
      if (!decoder || fseek(input, 32, SEEK_SET))
      {
         rvp8_video_close(decoder);
         return 0;
      }
      allocations = 0;
      fail_malloc = failure;
      while (fread(header, 1, 12, input) == 12)
      {
         unsigned size = little32(header);
         if (size > capacity || fread(packet, 1, size, input) != size)
         {
            ret = -2;
            break;
         }
         ret = rvp8_video_decode(decoder, packet, size);
         if (ret != 0) break;
      }
      fail_malloc = 0;
      rvp8_video_close(decoder);
      if ((ret != 0 && allocations != failure) || ret == -2
            || (allocations >= failure && ret == 0))
         return 0;
   }
   return 1;
}

int main(int argc, char **argv)
{
   FILE *input = NULL, *reference = NULL;
   rvp8_video *decoder = NULL;
   uint8_t header[32], *packet = NULL, *row = NULL;
   size_t capacity = 0, row_capacity = 0;
   unsigned frames = 0, packets = 0;
   int result = 1;
   if (argc != 3) return 2;
   input = fopen(argv[1], "rb");
   reference = fopen(argv[2], "rb");
   decoder = rvp8_video_open();
   if (!input || !reference || !decoder
         || fread(header, 1, 32, input) != 32
         || memcmp(header, "DKIF", 4) || memcmp(header + 8, "VP80", 4))
      goto done;
   copied = 0;
   while (fread(header, 1, 12, input) == 12)
   {
      unsigned size = little32(header), plane;
      if (!size || size > 16 * 1024 * 1024) goto done;
      if (size > capacity)
      {
         uint8_t *next = (uint8_t*)realloc(packet, size);
         if (!next) goto done;
         packet = next;
         capacity = size;
      }
      if (fread(packet, 1, size, input) != size
            || rvp8_video_decode(decoder, packet, size) != 0)
         goto done;
      packets++;
      if (!(packet[0] & 16)) continue;
      for (plane = 0; plane < 3; plane++)
      {
         int stride, width, height, y;
         const uint8_t *pixels = rvp8_video_plane(decoder, plane,
               &stride, &width, &height);
         if (!pixels || width <= 0 || height <= 0) goto done;
         if ((size_t)width > row_capacity)
         {
            uint8_t *next = (uint8_t*)realloc(row, (size_t)width);
            if (!next) goto done;
            row = next;
            row_capacity = (size_t)width;
         }
         for (y = 0; y < height; y++)
            if (fread(row, 1, (size_t)width, reference) != (size_t)width
                  || memcmp(row, pixels + (size_t)y * stride, (size_t)width))
            {
               fprintf(stderr, "frame %u plane %u row %d differs\n", frames, plane, y);
               goto done;
            }
      }
      frames++;
   }
   if (!frames || ferror(input) || fgetc(reference) != EOF
         || ferror(reference) || copied)
      goto done;
   printf("VP8 reference: %u shown, %u coded, %lu large-copy bytes\n",
         frames, packets, (unsigned long)copied);
   if (!allocation_failures(input, packet, capacity)) goto done;
   result = 0;
done:
   if (result) fprintf(stderr, "VP8 reference failed after %u frames (%lu copied bytes)\n",
         frames, (unsigned long)copied);
   if (input) fclose(input);
   if (reference) fclose(reference);
   rvp8_video_close(decoder);
   free(packet);
   free(row);
   return result;
}

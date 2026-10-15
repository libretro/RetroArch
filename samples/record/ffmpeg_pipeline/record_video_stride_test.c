/* The production row packer at contiguous and split ring boundaries. */
#define record_ffmpeg video_stride_driver
#include "../../../record/drivers/record_ffmpeg.c"
#undef record_ffmpeg

int record_video_stride_test_run(void)
{
   unsigned mirrored, negative, padded, rows, offset;
   unsigned cases = 0;
   for (mirrored = 0; mirrored < 2; mirrored++)
      for (negative = 0; negative < 2; negative++)
         for (padded = 0; padded < 2; padded++)
            for (rows = 1; rows <= 4; rows += 3)
               for (offset = 0; offset < 3; offset++)
               {
                  ffmpeg_t *ff = (ffmpeg_t*)calloc(1, sizeof(*ff));
                  uint8_t input[64], original[64], expected[48], actual[48];
                  uint8_t *padding;
                  const uint8_t *source;
                  size_t advance;
                  unsigned y, x, stride = padded ? 16 : 12;
                  bool ok;
                  if (!ff)
                     return 1;
                  ok = mirrored ? retro_spsc_init_mirrored(&ff->video_fifo, 64)
                     : retro_spsc_init(&ff->video_fifo, 64);
                  if (!ok) { free(ff); return 1; }
                  advance = offset == 0 ? 0 : ff->video_fifo.capacity - (offset == 1 ? 1 : 19);
                  padding = (uint8_t*)calloc(ff->video_fifo.capacity, 1);
                  if (!padding) { retro_spsc_free(&ff->video_fifo); free(ff); return 1; }
                  retro_spsc_write(&ff->video_fifo, padding, advance);
                  retro_spsc_read(&ff->video_fifo, padding, advance);
                  free(padding);
                  memset(input, 0xa5, sizeof(input));
                  for (y = 0; y < rows; y++)
                     for (x = 0; x < 12; x++)
                     {
                        unsigned physical = negative ? rows - 1 - y : y;
                        input[physical * stride + x] = expected[y * 12 + x] = (uint8_t)(y * 17 + x);
                     }
                  memcpy(original, input, sizeof(input));
                  source = input + (negative ? (rows - 1) * stride : 0);
                  ffmpeg_video_fifo_write(ff, source,
                        negative ? -(ptrdiff_t)stride : (ptrdiff_t)stride, rows, 12);
                  ok = retro_spsc_read_avail(&ff->video_fifo) == rows * 12;
                  retro_spsc_read(&ff->video_fifo, actual, rows * 12);
                  ok = ok && !memcmp(actual, expected, rows * 12)
                     && !memcmp(input, original, sizeof(input));
                  retro_spsc_free(&ff->video_fifo);
                  free(ff);
                  if (!ok)
                  {
                     fprintf(stderr, "stride case failed: mirrored=%u negative=%u padded=%u rows=%u offset=%u\n",
                           mirrored, negative, padded, rows, offset);
                     return 1;
                  }
                  cases++;
               }
   printf("[pass] %u signed-stride ring boundary cases\n", cases);
   return 0;
}

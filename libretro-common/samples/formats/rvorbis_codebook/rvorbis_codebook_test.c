/* rvorbis' codebook size bound: libvorbis' limit, so every table
 * sized from an accepted codebook stays under 2^24 entries, and a real
 * stream still decodes in full. */

#include <stdio.h>

#include "../../../formats/vorbis/rvorbis.c"
#include "../../audio/audio_mixer_loop/ogg_fixture.h"

static int failures = 0;

#define CHECK(c) do { if (!(c)) { \
   fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c); failures++; } } while (0)

static void test_bounds(void)
{
   int d;
   CHECK( codebook_size_valid(1, 1));
   CHECK(!codebook_size_valid(0, 1));
   CHECK(!codebook_size_valid(1, 0));
   CHECK( codebook_size_valid(1, (1 << 23) - 1));
   CHECK(!codebook_size_valid(1, 1 << 23));
   CHECK( codebook_size_valid(65535, 255));
   CHECK(!codebook_size_valid(65535, 256));
   CHECK(!codebook_size_valid(65535, (1 << 24) - 1));

   /* for every dimension count, the most entries it may have keeps the
    * product under 2^24, and one more is refused */
   for (d = 1; d <= 65535; d++)
   {
      int e = (1 << (24 - ilog(d))) - 1;
      if (e < 1)
      {
         CHECK(!codebook_size_valid(d, 1));
         continue;
      }
      if (!codebook_size_valid(d, e) || codebook_size_valid(d, e + 1)
            || (unsigned long)d * (unsigned long)e >= (1ul << 24))
      {
         fprintf(stderr, "FAIL dimensions %d, entries %d\n", d, e);
         failures++;
         break;
      }
   }
}

static void test_fixture_decodes(void)
{
   int err = 0;
   int frames = 0;
   int got;
   float out[512];
   rvorbis *v = rvorbis_open_memory(ogg_fixture, OGG_FIXTURE_SIZE, &err,
         NULL);
   CHECK(v != NULL);
   if (!v)
      return;
   while ((got = rvorbis_get_samples_float_interleaved(v,
               OGG_FIXTURE_CHANNELS, out, 512)) > 0)
      frames += got;
   CHECK(frames == OGG_FIXTURE_FRAMES);
   rvorbis_close(v);
}

int main(void)
{
   test_bounds();
   test_fixture_decodes();
   printf("rvorbis_codebook: %s (%d failures)\n",
         failures ? "FAIL" : "PASS", failures);
   return failures ? 1 : 0;
}

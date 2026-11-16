/* rvorbis' codebook size bound: libvorbis' limit, so every table
 * sized from an accepted codebook stays under 2^24 entries, and a real
 * stream still decodes in full.  A sparse codebook with no entry in use
 * and a lookup table frees the table's scratch (LeakSanitizer). */

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

/* Vorbis packs header fields from the low bit up */
struct bitw { unsigned char *p; int bit; };

static void put_bits(struct bitw *w, uint32_t v, int n)
{
   int i;
   for (i = 0; i < n; i++, w->bit++)
      if ((v >> i) & 1)
         w->p[w->bit >> 3] |= (unsigned char)(1 << (w->bit & 7));
}

static void test_sparse_unused_lookup(void)
{
   static const unsigned char id[30] = {
      1, 'v', 'o', 'r', 'b', 'i', 's', 0, 0, 0, 0,   /* version 0       */
      1, 0x44, 0xAC, 0, 0,                         /* mono, 44100 Hz  */
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0xB8, 1                                      /* blocks 256/2048 */
   };
   unsigned char setup[128];
   struct bitw w;
   int i, err = 0;
   rvorbis *v;

   memset(setup, 0, sizeof(setup));
   memcpy(setup, "\005vorbis", 7);
   w.p   = setup;
   w.bit = 7 * 8;
   put_bits(&w, 0, 8);                  /* one codebook             */
   put_bits(&w, 0x564342, 24);          /* its sync pattern         */
   put_bits(&w, 1, 16);                 /* 1 dimension              */
   put_bits(&w, 8, 24);                 /* 8 entries                */
   put_bits(&w, 0, 1);                  /* not ordered              */
   put_bits(&w, 1, 1);                  /* sparse                   */
   for (i = 0; i < 8; i++)
      put_bits(&w, 0, 1);               /* no entry in use          */
   put_bits(&w, 1, 4);                  /* lookup type 1            */
   put_bits(&w, 0, 32);                 /* minimum                  */
   put_bits(&w, 0, 32);                 /* delta                    */
   put_bits(&w, 3, 4);                  /* 4-bit values             */
   put_bits(&w, 0, 1);                  /* no sequence              */
   for (i = 0; i < 8; i++)
      put_bits(&w, (uint32_t)i, 4);     /* the lookup table         */

   /* what follows the codebook is not a valid setup; the codebook is
    * read either way */
   v = rvorbis_open_packets(id, (int)sizeof(id), setup, (int)sizeof(setup),
         &err, NULL);
   if (v)
      rvorbis_close(v);
}

int main(void)
{
   test_bounds();
   test_fixture_decodes();
   test_sparse_unused_lookup();
   printf("rvorbis_codebook: %s (%d failures)\n",
         failures ? "FAIL" : "PASS", failures);
   return failures ? 1 : 0;
}

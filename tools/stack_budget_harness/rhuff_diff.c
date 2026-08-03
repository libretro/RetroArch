/* Differential fuzz for the rhuff decoder-side entry points touched
 * by the scratch relocation: for a deterministic stream of inputs --
 * valid canonical length sets, corrupt ones, and raw noise fed to the
 * packed-tree reader -- print a running hash of every return code,
 * every lookup table, and every lengths array.  Built against the
 * baseline and the patched encoding_huffman.c; identical behavior
 * means identical final hashes. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <encodings/huffman.h>

static uint32_t fnv1a(uint32_t h, const void *p, size_t n)
{
   const uint8_t *b = (const uint8_t *)p;
   size_t i;
   for (i = 0; i < n; i++) { h ^= b[i]; h *= 0x01000193u; }
   return h;
}

static uint32_t rng_s = 0x1234567u;
static uint32_t rng(void) { rng_s = rng_s * 1664525u + 1013904223u; return rng_s; }

static uint16_t lookup16[1 << 16];
static uint8_t  noise[4096];

int main(void)
{
   uint32_t h = 0x811c9dc5u;
   int iter;
   rhuff_dec_t d;

   /* dec_build over random length sets: sometimes a valid canonical
    * tree (built by splitting kraft budget), mostly arbitrary. */
   for (iter = 0; iter < 4000; iter++)
   {
      uint32_t ncodes = 1 + (rng() % 2048);
      uint32_t maxb   = 1 + (rng() % 16);
      int r, i;
      memset(lookup16, 0xAA, sizeof(lookup16));
      r = rhuff_dec_init(&d, ncodes, maxb, lookup16,
            (size_t)1 << maxb);
      h = fnv1a(h, &r, sizeof(r));
      if (r != RHUFF_OK) continue;
      if (iter & 1)
      {
         /* arbitrary lengths, mostly invalid trees */
         for (i = 0; i < (int)ncodes; i++)
            d.lengths[i] = (uint8_t)(rng() % (maxb + 2));
      }
      else
      {
         /* valid-ish: all codes same length ceil(log2(ncodes)) when
          * that is complete, else leave a mix the checker rejects
          * deterministically */
         uint32_t L = 0; while ((1u << L) < ncodes) L++;
         if (L == 0) L = 1;
         if (L <= maxb && (1u << L) >= ncodes)
            for (i = 0; i < (int)ncodes; i++)
               d.lengths[i] = (uint8_t)(((1u << L) == ncodes || i) ? L : L);
         else
            for (i = 0; i < (int)ncodes; i++)
               d.lengths[i] = (uint8_t)(rng() % (maxb + 1));
      }
      r = rhuff_dec_build(&d);
      h = fnv1a(h, &r, sizeof(r));
      if (r == RHUFF_OK)
         h = fnv1a(h, lookup16, ((size_t)1 << maxb) * sizeof(uint16_t));
   }

   /* read_tree_packed over raw noise: both builds must agree on the
    * return code and, on success, the resulting tree. */
   for (iter = 0; iter < 4000; iter++)
   {
      rhuff_bits_t b;
      size_t n = 16 + (rng() % (sizeof(noise) - 16));
      size_t i; int r;
      for (i = 0; i < n; i++) noise[i] = (uint8_t)rng();
      memset(lookup16, 0x55, sizeof(lookup16));
      r = rhuff_dec_init(&d, 256, 16, lookup16, (size_t)1 << 16);
      if (r != RHUFF_OK) { h = fnv1a(h, &r, sizeof(r)); continue; }
      rhuff_bits_init(&b, noise, n);
      r = rhuff_read_tree_packed(&d, &b);
      h = fnv1a(h, &r, sizeof(r));
      if (r == RHUFF_OK)
      {
         h = fnv1a(h, d.lengths, sizeof(d.lengths));
         h = fnv1a(h, lookup16, sizeof(lookup16));
      }
   }

   printf("rhuff fuzz hash=%08x\n", h);
   return 0;
}

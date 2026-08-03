/* Differential PCM harness: decode an Ogg Opus file with ropus through
 * both the s16 (fixed/q) and f32 (float) pipelines and print FNV-1a
 * hashes of the output.  Built once against the baseline ropus.c and
 * once against the patched one; byte-identical output means identical
 * hashes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <formats/ropus.h>

static uint32_t fnv1a(uint32_t h, const void *p, size_t n)
{
   const uint8_t *b = (const uint8_t *)p;
   size_t i;
   for (i = 0; i < n; i++) { h ^= b[i]; h *= 0x01000193u; }
   return h;
}

/* Minimal Ogg page walker: returns packets in order via callback. */
typedef int (*pkt_cb)(const uint8_t *pkt, size_t len, void *ud);

static int ogg_walk(const uint8_t *d, size_t n, pkt_cb cb, void *ud)
{
   size_t off = 0;
   static uint8_t pkt[1 << 20];
   size_t pkt_len = 0;
   while (off + 27 <= n)
   {
      uint8_t nsegs; const uint8_t *lace; size_t body, i;
      if (memcmp(d + off, "OggS", 4)) return -1;
      nsegs = d[off + 26];
      if (off + 27 + nsegs > n) return -1;
      lace = d + off + 27;
      body = off + 27 + nsegs;
      for (i = 0; i < nsegs; i++)
      {
         uint8_t l = lace[i];
         if (body + l > n) return -1;
         if (pkt_len + l > sizeof(pkt)) return -1;
         memcpy(pkt + pkt_len, d + body, l); pkt_len += l; body += l;
         if (l < 255)   /* packet complete */
         {
            if (cb(pkt, pkt_len, ud)) return -1;
            pkt_len = 0;
         }
      }
      off = body;
   }
   return 0;
}

typedef struct
{
   ropus_t *o16, *o32;
   int      idx;
   uint32_t h16, h32;
   long     n16, n32;
} state_t;

static int on_pkt(const uint8_t *pkt, size_t len, void *ud)
{
   state_t *s = (state_t *)ud;
   static int16_t pcm16[5760 * 2];
   static float   pcm32[5760 * 2];
   if (s->idx == 0)                       /* OpusHead */
   {
      s->o16 = ropus_open(pkt, len);
      s->o32 = ropus_open(pkt, len);
      if (!s->o16 || !s->o32) { fprintf(stderr, "open failed\n"); return 1; }
   }
   else if (s->idx == 1) { /* OpusTags: skip */ }
   else
   {
      int ch = (int)ropus_channels(s->o16);
      int r16 = ropus_decode_s16(s->o16, pkt, len, pcm16, 5760);
      int r32 = ropus_decode_f32(s->o32, pkt, len, pcm32, 5760);
      if (r16 < 0 || r32 < 0) { fprintf(stderr, "decode err %d %d @pkt %d\n", r16, r32, s->idx); return 1; }
      s->h16 = fnv1a(s->h16, pcm16, (size_t)r16 * ch * sizeof(int16_t));
      s->h32 = fnv1a(s->h32, pcm32, (size_t)r32 * ch * sizeof(float));
      s->n16 += r16; s->n32 += r32;
   }
   s->idx++;
   return 0;
}

int main(int argc, char **argv)
{
   FILE *f; long n; uint8_t *buf; state_t s;
   if (argc < 2) return 2;
   if (!(f = fopen(argv[1], "rb"))) return 2;
   fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
   buf = (uint8_t *)malloc((size_t)n);
   if (fread(buf, 1, (size_t)n, f) != (size_t)n) return 2;
   fclose(f);
   memset(&s, 0, sizeof(s));
   s.h16 = s.h32 = 0x811c9dc5u;
   if (ogg_walk(buf, (size_t)n, on_pkt, &s)) return 1;
   printf("%s pkts=%d s16: n=%ld h=%08x  f32: n=%ld h=%08x\n",
      argv[1], s.idx, s.n16, s.h16, s.n32, s.h32);
   ropus_close(s.o16); ropus_close(s.o32); free(buf);
   return 0;
}

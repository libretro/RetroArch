/* Combined differential harness for the final allowlist sweep.
 * Modes (argv[1]):
 *   vorbis <file.ogg>   decode via s16 and float paths, print hashes
 *   zstd                encode/decode round-trip over deterministic
 *                       payloads at several levels, print hashes
 *   config              build a config, dump it, hash the bytes
 *   mem                 print mem_stats_total/free (order-of-magnitude
 *                       check on a live /proc, exact diff done by
 *                       comparing baseline vs patched output)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static uint32_t fnv1a(uint32_t h, const void *p, size_t n)
{
   const uint8_t *b = (const uint8_t *)p;
   size_t i;
   for (i = 0; i < n; i++) { h ^= b[i]; h *= 0x01000193u; }
   return h;
}
static uint32_t rng_s = 0xC0FFEEu;
static uint32_t rng(void) { rng_s = rng_s * 1664525u + 1013904223u; return rng_s; }

#ifdef MODE_VORBIS
#include <formats/rvorbis.h>
int main(int argc, char **argv)
{
   FILE *fp; long n; uint8_t *d; int err = 0;
   rvorbis *v, *v2;
   static int16_t s16[4096 * 8];
   static float   f32[4096 * 8];
   uint32_t h16 = 0x811c9dc5u, h32 = 0x811c9dc5u;
   long n16 = 0, n32 = 0;
   int ch, r;

   if (argc < 3) return 2;
   fp = fopen(argv[2], "rb"); if (!fp) return 2;
   fseek(fp, 0, SEEK_END); n = ftell(fp); fseek(fp, 0, SEEK_SET);
   d = (uint8_t *)malloc((size_t)n);
   if (fread(d, 1, (size_t)n, fp) != (size_t)n) return 2;
   fclose(fp);

   v  = rvorbis_open_memory(d, (int)n, &err, NULL);
   v2 = rvorbis_open_memory(d, (int)n, &err, NULL);
   if (!v || !v2) { fprintf(stderr, "open err %d\n", err); return 1; }
   ch = 2; /* interleave into stereo regardless of source */
   while ((r = rvorbis_get_samples_s16_interleaved(v, ch, s16,
               (int)(sizeof(s16)/sizeof(s16[0])))) > 0)
   { h16 = fnv1a(h16, s16, (size_t)r * ch * sizeof(int16_t)); n16 += r; }
   while ((r = rvorbis_get_samples_float_interleaved(v2, ch, f32,
               (int)(sizeof(f32)/sizeof(f32[0])))) > 0)
   { h32 = fnv1a(h32, f32, (size_t)r * ch * sizeof(float)); n32 += r; }
   printf("%s s16: n=%ld h=%08x  f32: n=%ld h=%08x\n",
      argv[2], n16, h16, n32, h32);
   return 0;
}
#endif

#ifdef MODE_ZSTD
#include <encodings/rzstd.h>
int main(void)
{
   static uint8_t src[1 << 18], enc[1 << 19], dec[1 << 18];
   uint32_t h = 0x811c9dc5u;
   int trial;
   for (trial = 0; trial < 24; trial++)
   {
      size_t n = 64 + (rng() % (sizeof(src) - 64));
      size_t i, wrote = 0, got = 0;
      int level = 1 + (trial % 9), e;
      switch (trial % 3)
      {
         case 0: for (i = 0; i < n; i++) src[i] = (uint8_t)rng(); break;
         case 1: for (i = 0; i < n; i++) src[i] = (uint8_t)(i & 0x3F); break;
         default:
            for (i = 0; i < n; i++)
               src[i] = (uint8_t)"the quick brown fox jumps over "[i % 31];
            break;
      }
      e = rzstd_encode(enc, sizeof(enc), src, n, level, &wrote);
      if (e != RZSTD_PROCESS_END) { printf("enc fail %d\n", trial); return 1; }
      h = fnv1a(h, enc, wrote);
      e = rzstd_decode(dec, n, enc, wrote, &got);
      if (e != RZSTD_PROCESS_END || got != n || memcmp(dec, src, n))
      { printf("dec fail %d e=%d got=%zu\n", trial, e, got); return 1; }
      h = fnv1a(h, dec, n);
   }
   printf("zstd roundtrip hash=%08x\n", h);
   return 0;
}
#endif

#ifdef MODE_CONFIG
#include <file/config_file.h>
int main(void)
{
   static const char *text =
      "zeta = \"3\"\nalpha = \"1\"\n#reference \"base.cfg\"\n"
      "video_driver = \"vulkan\"\nmenu_scale = \"1.5\"\n"
      "beta = \"two words here\"\n";
   config_file_t *c = config_file_new_from_string(strdup(text), "t.cfg");
   FILE *out; long n; uint8_t *b; uint32_t h;
   int pass;
   if (!c) return 1;
   for (pass = 0; pass < 2; pass++)
   {
      out = fopen("/tmp/cfg_dump_out.txt", "wb+");
      config_file_dump(c, out, pass);   /* unsorted, then sorted */
      fseek(out, 0, SEEK_END); n = ftell(out); fseek(out, 0, SEEK_SET);
      b = (uint8_t *)malloc((size_t)n);
      if (fread(b, 1, (size_t)n, out) != (size_t)n) return 1;
      fclose(out);
      h = fnv1a(0x811c9dc5u, b, (size_t)n);
      printf("config dump sort=%d n=%ld h=%08x\n", pass, n, h);
      free(b);
   }
   config_file_free(c);
   return 0;
}
#endif

#ifdef MODE_MEM
#include <memory/mem_stats.h>
int main(void)
{
   /* exact values move between runs on a live system; the diff is that
    * baseline and patched agree on the same reading when run
    * back-to-back, and that both parse to sane nonzero values. */
   uint64_t t = mem_stats_total(), f = mem_stats_free();
   printf("mem total=%llu free=%llu sane=%d\n",
      (unsigned long long)t, (unsigned long long)f,
      t > (1ull << 20) && f > 0 && f <= t);
   return 0;
}
#endif

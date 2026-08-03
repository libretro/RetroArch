/* Differential harness for rflac: decode a .flac file through the
 * header path (rflac_new -> rflac_open_with_metadata_private), then
 * strip the header and decode the bare frames through the raw path
 * (rflac_new_raw -> rflac__alloc_raw), hashing s16 PCM from both.
 * Requires a fixed-blocksize stream (STREAMINFO min == max). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <formats/rflac.h>

static uint32_t fnv1a(uint32_t h, const void *p, size_t n)
{
   const uint8_t *b = (const uint8_t *)p;
   size_t i;
   for (i = 0; i < n; i++) { h ^= b[i]; h *= 0x01000193u; }
   return h;
}

static uint32_t rd24(const uint8_t *p) { return (p[0]<<16)|(p[1]<<8)|p[2]; }

static long push_decode(rflac_t *f, const uint8_t *blob, size_t len,
      size_t chunk, uint32_t *hash)
{
   static int16_t out[1 << 20];
   size_t off = 0, got = 0;
   int e = RFLAC_PROCESS_NEXT;
   rflac_set_out_s16(f, out, sizeof(out)/sizeof(out[0]) / 2);
   while (off < len)
   {
      size_t take = (chunk && chunk < len - off) ? chunk : len - off;
      size_t rd = 0, wr = 0;
      rflac_set_in(f, blob + off, take);
      for (;;)
      {
         e = rflac_process(f, &rd, &wr);
         got += wr;
         if (e != RFLAC_PROCESS_NEXT || wr == 0)
            break;
      }
      if (e == RFLAC_PROCESS_ERROR) return -1;
      off += rd;
      if (e == RFLAC_PROCESS_END) break;
      if (rd == 0 && wr == 0) break;
   }
   *hash = fnv1a(0x811c9dc5u, out, got * 2 * sizeof(int16_t));
   return (long)got;
}

int main(int argc, char **argv)
{
   FILE *fp; long n; uint8_t *buf;
   uint32_t h_hdr = 0, h_raw = 0;
   long g_hdr, g_raw;
   size_t off; int last;
   rflac_format_t fmt;
   rflac_t *f;

   if (argc < 2) return 2;
   if (!(fp = fopen(argv[1], "rb"))) return 2;
   fseek(fp, 0, SEEK_END); n = ftell(fp); fseek(fp, 0, SEEK_SET);
   buf = (uint8_t *)malloc((size_t)n);
   if (fread(buf, 1, (size_t)n, fp) != (size_t)n) return 2;
   fclose(fp);

   if (memcmp(buf, "fLaC", 4)) { fprintf(stderr, "not flac\n"); return 2; }

   /* STREAMINFO: first metadata block. */
   {
      const uint8_t *si = buf + 8;              /* skip fLaC + block hdr */
      uint32_t minbs = (si[0] << 8) | si[1];
      uint32_t maxbs = (si[2] << 8) | si[3];
      uint32_t sr    = rd24(si + 10) >> 4;
      uint32_t ch    = ((si[12] >> 1) & 7) + 1;
      uint32_t bps   = (((si[12] & 1) << 4) | (si[13] >> 4)) + 1;
      if (minbs != maxbs) { fprintf(stderr, "variable blocksize\n"); return 2; }
      fmt.sample_rate = sr; fmt.channels = ch;
      fmt.bits_per_sample = bps; fmt.block_size = minbs;
   }

   /* header path, pushed in awkward chunk sizes */
   if (!(f = rflac_new())) return 3;
   g_hdr = push_decode(f, buf, (size_t)n, 977, &h_hdr);
   rflac_free(f);

   /* raw path: skip metadata blocks, feed bare frames */
   off = 4; last = 0;
   while (!last && off + 4 <= (size_t)n)
   {
      uint32_t blen = rd24(buf + off + 1);
      last = buf[off] >> 7;
      off += 4 + blen;
   }
   if (!(f = rflac_new_raw(&fmt))) return 3;
   g_raw = push_decode(f, buf + off, (size_t)n - off, 511, &h_raw);
   rflac_free(f);

   printf("%s hdr: n=%ld h=%08x  raw: n=%ld h=%08x\n",
      argv[1], g_hdr, h_hdr, g_raw, h_raw);
   free(buf);
   return (g_hdr <= 0 || g_raw <= 0) ? 1 : 0;
}

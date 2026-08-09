/* Decodes CHD 'cdfl' hunks through the push-based rflac API and
 * compares against the sector data an independent reader produces.
 * Also feeds the same blob in small chunks, to exercise the rewind
 * path where a frame straddles two spans. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rflac.h>

static int decode(const uint8_t *blob, size_t len, size_t chunk,
      int16_t *out, size_t frames)
{
   rflac_format_t fmt;
   rflac_t       *f;
   size_t         off = 0;
   size_t         got = 0;

   fmt.sample_rate     = 44100;
   fmt.channels        = 2;
   fmt.bits_per_sample = 16;
   fmt.block_size      = 2352;

   if (!(f = rflac_new_raw(&fmt)))
      return -1;
   rflac_set_out_s16(f, out, frames);

   while (off < len && got < frames)
   {
      size_t take = (chunk && chunk < len - off) ? chunk : len - off;
      size_t rd, wr;
      int    e;
      rflac_set_in(f, blob + off, take);
      for (;;)
      {
         e = rflac_process(f, &rd, &wr);
         got += wr;
         if (e != RFLAC_PROCESS_NEXT || wr == 0)
            break;
      }
      if (e == RFLAC_PROCESS_ERROR) { rflac_free(f); return -2; }
      off += rd;
      if (e == RFLAC_PROCESS_END) break;
   }
   rflac_free(f);
   return (int)got;
}

int main(int argc, char **argv)
{
   FILE   *bf, *tf;
   long    blen, tlen;
   uint8_t *blob, *truth;
   int16_t *out;
   size_t   frames = 4704;
   int      got;
   size_t   i;
   int      fail = 0;

   if (argc < 3) return 2;
   bf = fopen(argv[1], "rb"); tf = fopen(argv[2], "rb");
   if (!bf || !tf) return 1;
   fseek(bf,0,SEEK_END); blen=ftell(bf); fseek(bf,0,SEEK_SET);
   blob = (uint8_t*)malloc((size_t)blen);
   if (fread(blob,1,(size_t)blen,bf)!=(size_t)blen) return 1; fclose(bf);
   fseek(tf,0,SEEK_END); tlen=ftell(tf); fseek(tf,0,SEEK_SET);
   truth = (uint8_t*)malloc((size_t)tlen);
   if (fread(truth,1,(size_t)tlen,tf)!=(size_t)tlen) return 1; fclose(tf);

   out = (int16_t*)malloc(frames*2*sizeof(int16_t));

   {
      static const size_t chunks[] = { 0, 4096, 512, 64, 7 };
      size_t c;
      for (c = 0; c < sizeof(chunks)/sizeof(chunks[0]); c++)
      {
         memset(out, 0, frames*2*sizeof(int16_t));
         got = decode(blob,(size_t)blen,chunks[c],out,frames);
         /* CHD stores the samples byte-swapped against the sector. */
         {
            int ok = (got == (int)frames);
            const uint8_t *p = (const uint8_t*)out;
            for (i = 0; ok && i < frames*4; i += 2)
               if (p[i] != truth[i+1] || p[i+1] != truth[i]) ok = 0;
            if (chunks[c]) printf("  chunk=%-6lu frames=%-6d %s\n",
                   (unsigned long)chunks[c], got, ok ? "byte-exact" : "MISMATCH");
            else printf("  chunk=whole  frames=%-6d %s\n", got, ok ? "byte-exact" : "MISMATCH");
            if (!ok) fail = 1;
         }
      }
   }
   free(out); free(blob); free(truth);
   return fail;
}

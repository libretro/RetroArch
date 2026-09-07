/* Decodes a native FLAC stream (fLaC marker + metadata + frames)
 * through the push API's header path, at several chunk sizes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rflac.h>

int main(int argc, char **argv)
{
   FILE *sf; long slen; uint8_t *stream;
   size_t chunk; int fail = 0;
   static const size_t chunks[] = { 0, 4096, 333, 64, 5 };
   size_t c;
   int16_t *ref = NULL; size_t ref_frames = 0;

   if (argc < 2) return 2;
   sf = fopen(argv[1],"rb"); if(!sf) return 1;
   fseek(sf,0,SEEK_END); slen=ftell(sf); fseek(sf,0,SEEK_SET);
   stream=(uint8_t*)malloc((size_t)slen);
   if (fread(stream,1,(size_t)slen,sf)!=(size_t)slen) return 1;
   fclose(sf);

   for (c = 0; c < sizeof(chunks)/sizeof(chunks[0]); c++)
   {
      rflac_t *f = rflac_new();
      const rflac_format_t *fmt;
      int16_t *out; size_t cap = 200000, got = 0, off = 0;
      int e = RFLAC_PROCESS_NEXT;

      chunk = chunks[c];
      out = (int16_t*)calloc(cap*2, sizeof(int16_t));
      rflac_set_out_s16(f, out, cap);

      while (off < (size_t)slen)
      {
         size_t take = (chunk && chunk < (size_t)slen - off) ? chunk : (size_t)slen - off;
         size_t rd, wr;
         rflac_set_in(f, stream + off, take);
         for (;;)
         {
            e = rflac_process(f, &rd, &wr);
            got += wr;
            if (e != RFLAC_PROCESS_NEXT || wr == 0) break;
         }
         if (e == RFLAC_PROCESS_ERROR) break;
         off += rd;
         if (e == RFLAC_PROCESS_END) break;
      }
      fmt = rflac_format(f);
      if (c == 0)
      {
         ref = out; ref_frames = got;
         printf("  chunk=whole  rate=%u ch=%u bits=%u block=%u frames=%lu\n",
                fmt?fmt->sample_rate:0, fmt?fmt->channels:0,
                fmt?fmt->bits_per_sample:0, fmt?fmt->block_size:0,
                (unsigned long)got);
      }
      else
      {
         int same = (got == ref_frames)
                 && memcmp(out, ref, got*2*sizeof(int16_t)) == 0;
         printf("  chunk=%-6lu frames=%-8lu %s\n", (unsigned long)chunk,
                (unsigned long)got, same ? "identical to whole-span decode" : "MISMATCH");
         if (!same) fail = 1;
         free(out);
      }
      rflac_free(f);
   }
   free(ref); free(stream);
   return fail;
}

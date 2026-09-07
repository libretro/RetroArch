/* Drives audio_transfer's FLAC arm end to end and compares the PCM
 * against a reference decode of the same stream. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/audio.h>
#include <formats/rflac_next.h>

int main(int argc, char **argv)
{
   FILE *f; long n; uint8_t *buf;
   void *h; unsigned ch=0, rate=0; uint64_t total=0;
   int16_t *out; size_t cap=200000, got=0;
   int16_t *ref; size_t refgot=0;
   int e;

   if (argc<2) return 2;
   f=fopen(argv[1],"rb"); if(!f) return 1;
   fseek(f,0,SEEK_END); n=ftell(f); fseek(f,0,SEEK_SET);
   buf=(uint8_t*)malloc((size_t)n);
   if (fread(buf,1,(size_t)n,f)!=(size_t)n) return 1; fclose(f);

   /* reference: the push API directly */
   {
      rflac_t *d = rflac_new();
      size_t rd, wr;
      ref = (int16_t*)calloc(cap*2,sizeof(int16_t));
      rflac_set_out_s16(d, ref, cap);
      rflac_set_in(d, buf, (size_t)n);
      while ((e = rflac_process(d,&rd,&wr)) == RFLAC_PROCESS_NEXT && wr)
         refgot += wr;
      refgot += wr;
      rflac_free(d);
   }

   h = audio_transfer_new(AUDIO_TYPE_FLAC);
   if (!h) { printf("  audio_transfer_new failed\n"); return 1; }
   audio_transfer_set_buffer_ptr(h, AUDIO_TYPE_FLAC, buf, (size_t)n);
   if (!audio_transfer_start(h, AUDIO_TYPE_FLAC))
   { printf("  audio_transfer_start failed\n"); return 1; }
   if (!audio_transfer_info(h, AUDIO_TYPE_FLAC, &ch, &rate, &total))
   { printf("  audio_transfer_info failed\n"); return 1; }
   printf("  info: channels=%u rate=%u total=%lu\n", ch, rate,
          (unsigned long)total);

   out=(int16_t*)calloc(cap*2,sizeof(int16_t));
   for (;;)
   {
      size_t produced = 0;
      int r = audio_transfer_read_s16(h, AUDIO_TYPE_FLAC,
                                      out + got*ch, 1024, &produced);
      if (r == AUDIO_PROCESS_ERROR || r == AUDIO_PROCESS_ERROR_END) break;
      got += produced;
      if (!produced || got + 1024 > cap) break;
      if (r == AUDIO_PROCESS_END) break;
   }
   printf("  decoded through audio_transfer, ref frames=%lu\n",
          (unsigned long)refgot);
   printf("  first 1024 frames match reference: %s\n",
          memcmp(out, ref, 1024*ch*sizeof(int16_t))==0 ? "yes" : "NO");
   free(out); free(ref); free(buf);
   audio_transfer_free(h, AUDIO_TYPE_FLAC);
   return 0;
}

int path_is_directory(const char *p){(void)p;return 0;}
int path_mkdir(const char *d){(void)d;return 0;}

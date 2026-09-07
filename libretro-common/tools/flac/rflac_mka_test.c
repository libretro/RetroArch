/* Drives audio_transfer's Matroska FLAC arm and compares against a
 * reference decode of the same frames as a native stream. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/audio.h>
#include <formats/rflac.h>

static uint8_t *slurp(const char *path, size_t *len)
{
   FILE *f = fopen(path,"rb"); long n; uint8_t *b;
   if (!f) return NULL;
   fseek(f,0,SEEK_END); n=ftell(f); fseek(f,0,SEEK_SET);
   b=(uint8_t*)malloc((size_t)n);
   if (fread(b,1,(size_t)n,f)!=(size_t)n) { fclose(f); return NULL; }
   fclose(f); *len=(size_t)n; return b;
}

int main(int argc, char **argv)
{
   uint8_t *mka, *nat; size_t mlen, nlen;
   int16_t *ref, *out; size_t cap=200000, refgot=0, got=0;
   void *h; unsigned ch=0, rate=0; uint64_t total=0;

   if (argc<3) return 2;
   if (!(mka=slurp(argv[1],&mlen)) || !(nat=slurp(argv[2],&nlen))) return 1;

   /* reference: the same frames as a native stream */
   {
      rflac_t *d = rflac_new(); size_t rd,wr; int e;
      ref=(int16_t*)calloc(cap*2,sizeof(int16_t));
      rflac_set_out_s16(d, ref, cap);
      rflac_set_in(d, nat, nlen);
      while ((e=rflac_process(d,&rd,&wr))==RFLAC_PROCESS_NEXT && wr) refgot+=wr;
      refgot+=wr; rflac_free(d);
   }

   h = audio_transfer_new(AUDIO_TYPE_FLAC);
   if (!h) { printf("  new failed\n"); return 1; }
   audio_transfer_set_buffer_ptr(h, AUDIO_TYPE_FLAC, mka, mlen);
   if (!audio_transfer_start(h, AUDIO_TYPE_FLAC))
   { printf("  start failed (Matroska not detected?)\n"); return 1; }
   if (!audio_transfer_info(h, AUDIO_TYPE_FLAC, &ch, &rate, &total))
   { printf("  info failed\n"); return 1; }
   printf("  mka info: channels=%u rate=%u\n", ch, rate);

   out=(int16_t*)calloc(cap*2,sizeof(int16_t));
   for (;;)
   {
      size_t produced=0;
      int r = audio_transfer_read_s16(h, AUDIO_TYPE_FLAC,
                                      out+got*ch, 1024, &produced);
      if (r==AUDIO_PROCESS_ERROR || r==AUDIO_PROCESS_ERROR_END) break;
      got += produced;
      if (!produced || got+1024>cap) break;
      if (r==AUDIO_PROCESS_END) break;
   }
   printf("  mka frames=%lu  native reference frames=%lu\n",
          (unsigned long)got,(unsigned long)refgot);
   printf("  PCM identical: %s\n",
          (got && got==refgot
           && memcmp(out,ref,got*ch*sizeof(int16_t))==0) ? "yes" : "NO");
   free(out); free(ref); free(mka); free(nat);
   audio_transfer_free(h, AUDIO_TYPE_FLAC);
   return 0;
}
int path_is_directory(const char *p){(void)p;return 0;}
int path_mkdir(const char *d){(void)d;return 0;}

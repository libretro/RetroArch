/* Wraps a candidate headerless FLAC stream in a STREAMINFO built from
 * the FLAC spec, decodes it with rflac, and reports how many samples
 * came out. Sweeping offset and block size shows where a cdfl blob's
 * FLAC data actually starts. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rflac.h>

static void put_bits(unsigned char *b, int *bitpos, unsigned long v, int n)
{
   int i;
   for (i = n - 1; i >= 0; i--)
   {
      int bit = (int)((v >> i) & 1);
      if (bit) b[*bitpos >> 3] |= (unsigned char)(0x80 >> (*bitpos & 7));
      (*bitpos)++;
   }
}

int main(int argc, char **argv)
{
   FILE *f; long n; unsigned char *blob;
   unsigned offset, blocksize, channels, bits, rate;
   unsigned char hdr[42]; int bp;
   unsigned char *stream; size_t slen;
   rflac *fl; short *out; unsigned long got;

   if (argc < 7) { printf("usage: blob off blocksize ch bits rate\n"); return 2; }
   f = fopen(argv[1], "rb"); if (!f) return 1;
   fseek(f,0,SEEK_END); n = ftell(f); fseek(f,0,SEEK_SET);
   blob = malloc((size_t)n); if (fread(blob,1,(size_t)n,f)!=(size_t)n) return 1;
   fclose(f);

   offset    = (unsigned)atoi(argv[2]);
   blocksize = (unsigned)atoi(argv[3]);
   channels  = (unsigned)atoi(argv[4]);
   bits      = (unsigned)atoi(argv[5]);
   rate      = (unsigned)atoi(argv[6]);
   if (offset >= (unsigned)n) return 1;

   memset(hdr,0,sizeof(hdr));
   memcpy(hdr,"fLaC",4);
   bp = 32;
   put_bits(hdr,&bp,1,1);     /* last metadata block */
   put_bits(hdr,&bp,0,7);     /* STREAMINFO */
   put_bits(hdr,&bp,34,24);   /* length */
   put_bits(hdr,&bp,blocksize,16);
   put_bits(hdr,&bp,blocksize,16);
   put_bits(hdr,&bp,0,24);
   put_bits(hdr,&bp,0,24);
   put_bits(hdr,&bp,rate,20);
   put_bits(hdr,&bp,channels-1,3);
   put_bits(hdr,&bp,bits-1,5);
   put_bits(hdr,&bp,0,36);
   bp += 128;

   slen = 42 + ((size_t)n - offset);
   stream = malloc(slen);
   memcpy(stream,hdr,42);
   memcpy(stream+42, blob+offset, (size_t)n-offset);

   fl = rflac_open_memory(stream, slen);
   if (!fl) { printf("offset=%-5u bs=%-5u -> open failed\n", offset, blocksize); return 1; }
   out = malloc(65536*(size_t)channels*sizeof(short));
   got = (unsigned long)rflac_read_pcm_frames_s16(fl, 65536, out);
   printf("offset=%-5u bs=%-5u ch=%u bits=%u -> decoded %lu frames (%lu bytes)\n",
          offset, blocksize, channels, bits, got, got*channels*2);
   if (got) { FILE*o=fopen("/tmp/flacout.bin","wb"); fwrite(out,1,got*channels*2,o); fclose(o); }
   rflac_close(fl);
   return got ? 0 : 1;
}
int path_is_directory(const char*p){(void)p;return 0;}
int path_mkdir(const char*d){(void)d;return 0;}

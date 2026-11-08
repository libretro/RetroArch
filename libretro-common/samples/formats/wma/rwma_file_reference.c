/* Copyright (C) 2026 The RetroArch team. MIT license. */
#include <formats/rwma_stream.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static uint8_t reference[8192];
int main(int argc, char **argv)
{
   FILE *input, *pcm;
   uint8_t *encoded;
   rwma_stream *stream = NULL;
   rwma_frame frame;
   long length;
   unsigned pass;
   int status;
   double maximum = 0;
   size_t total = 0;
   if (argc != 3) return 1;
   input = fopen(argv[1], "rb"); pcm = fopen(argv[2], "rb");
   if (!input || !pcm || fseek(input,0,SEEK_END)) return 2;
   length = ftell(input);
   if (length <= 0 || fseek(input,0,SEEK_SET)) return 2;
   encoded = (uint8_t*)malloc((size_t)length);
   if (!encoded || fread(encoded,1,(size_t)length,input)!=(size_t)length) return 3;
   fclose(input);
   if (rwma_stream_open(&stream,encoded,(size_t)length,0)) return 4;
   for (pass=0; pass<2; pass++)
   {
      total = 0;
      while ((status=rwma_stream_next(stream,&frame))==RWMA_OK)
      {
         size_t bytes = (size_t)frame.samples*frame.channels*2;
         unsigned i, channel;
         if (bytes>sizeof(reference) || fread(reference,1,bytes,pcm)!=bytes) return 5;
         for (i=0; i<frame.samples; i++) for (channel=0; channel<frame.channels; channel++)
         {
            const uint8_t *p=reference+((size_t)i*frame.channels+channel)*2;
            int sample=p[0]|((unsigned)p[1]<<8);
            double difference;
            if (sample>=32768) sample-=65536;
            difference=fabs(frame.planes[channel][i]*32768.0-sample);
            if (difference>maximum) maximum=difference;
            if (!(difference<=0.51))
            {
               fprintf(stderr,"PCM mismatch frame %lu channel %u: %.9g LSB\n",
                     (unsigned long)(total+i),channel,difference);
               return 6;
            }
         }
         total+=frame.samples;
      }
      if (status!=RWMA_STREAM_END || !total || fgetc(pcm)!=EOF || ferror(pcm)) return 7;
      rwma_stream_rewind(stream);
      rewind(pcm);
   }
   printf("PASS %lu frames, reset, max %.9g S16 LSB\n",(unsigned long)total,maximum);
   fclose(pcm); rwma_stream_close(stream); free(encoded);
   return 0;
}

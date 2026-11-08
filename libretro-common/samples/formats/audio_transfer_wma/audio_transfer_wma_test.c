/* Copyright (C) 2026 The RetroArch team. MIT license. */
#include <audio/audio_mixer.h>
#include <formats/audio.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_RWMA
#define PI 3.14159265358979323846
static float output[2048], reference[2048], mixed[512];
static int16_t pcm[2048];
static void check(int valid, const char *what)
{
   if (!valid)
   {
      fprintf(stderr, "FAIL %s\n", what);
      exit(1);
   }
}
#include "wma_fixture.h"
static void expected(void)
{
   unsigned i;
   for (i = 0; i < 2048; i++)
   {
      double transform = -cos(PI / 2048 * (i + 0.5 + 1024) * 0.5);
      double window = sin((i + 0.5) * PI / 4096);
      reference[i] =
          (float)(256 * pow(10.0, 5.0) * (2.0 / 2048) / 32768 * transform * window);
   }
}
static void transfer_test(uint8_t *data, size_t size, size_t prefix)
{
   void *context = audio_transfer_new(AUDIO_TYPE_WMA);
   size_t got = 0, done = 0;
   unsigned rate = 0, channels = 0, i;
   uint64_t total = 123;
   int status;
   check(context != NULL, "WMA transfer enabled");
   audio_transfer_set_buffer_ptr(context, AUDIO_TYPE_WMA, data, size);
   audio_transfer_set_avail(context, AUDIO_TYPE_WMA, prefix);
   check(audio_transfer_start(context, AUDIO_TYPE_WMA), "container start");
   check(audio_transfer_info(context, AUDIO_TYPE_WMA, &channels, &rate, &total) &&
             channels == 1 && rate == 48000 && total == 0,
         "native stream metadata");
   status = audio_transfer_read_f32(context, AUDIO_TYPE_WMA, output, 73, &got);
   check(status == AUDIO_PROCESS_NEXT && got == 0, "frontier is starvation, not EOF");
   check(audio_transfer_buffer_read_end(context, AUDIO_TYPE_WMA) > prefix,
         "refill demand");
   audio_transfer_set_avail(context, AUDIO_TYPE_WMA, size);
   while (done < 2048)
   {
      size_t want = 2048 - done;
      if (want > 73)
         want = 73;
      status =
          audio_transfer_read_f32(context, AUDIO_TYPE_WMA, output + done, want, &got);
      check(status == AUDIO_PROCESS_NEXT && got == want,
            "consecutive partial frame reads");
      done += got;
   }
   for (i = 0; i < 2048; i++)
      check(fabs(output[i] - reference[i]) < 0.000002, "analytic PCM");
   check(audio_transfer_read_f32(context, AUDIO_TYPE_WMA, output, 1, &got) ==
                 AUDIO_PROCESS_END &&
             !got,
         "exact end without extra PCM");
   check(audio_transfer_seek(context, AUDIO_TYPE_WMA, 0),
         "allocation-free loop rewind");
   check(audio_transfer_read_s16(context, AUDIO_TYPE_WMA, pcm, 2048, &got) ==
                 AUDIO_PROCESS_NEXT &&
             got == 2048,
         "integer delivery");
   for (i = 0; i < 2048; i++)
      check(fabs(pcm[i] - reference[i] * 32768.0) < 0.55, "integer quantization");
   check(audio_transfer_buffer_tell(context, AUDIO_TYPE_WMA) <= size,
         "bounded read cursor");
   audio_transfer_free(context, AUDIO_TYPE_WMA);
}
static void mixer_test(const uint8_t *data, size_t size)
{
   audio_mixer_sound_t *sound;
   audio_mixer_voice_t *voice;
   uint8_t *owned = (uint8_t *)malloc(size);
   unsigned i;
   check(owned != NULL, "mixer source");
   memcpy(owned, data, size);
   audio_mixer_init(48000);
   sound = audio_mixer_load_wma(owned, size);
   check(sound != NULL, "WMA mixer loader");
   voice = audio_mixer_play(sound, false, 1.0f, "nearest", RESAMPLER_QUALITY_DONTCARE,
                            NULL);
   check(voice != NULL, "WMA mixer voice");
   memset(mixed, 0, sizeof(mixed));
   audio_mixer_mix(mixed, 256, 1.0f, false);
   for (i = 0; i < 256; i++)
   {
      check(fabs(mixed[i * 2] - reference[i]) < 0.000002, "left mono mapping");
      check(fabs(mixed[i * 2 + 1] - reference[i]) < 0.000002, "right mono mapping");
   }
   audio_mixer_stop(voice);
   audio_mixer_destroy(sound);
   audio_mixer_done();
}
static void implicit_xwma_test(void)
{
   uint8_t data[78], *p = data + 46;
   unsigned at = 0, version;
   for (version = 0; version < 2; version++)
   {
      void *context;
      size_t got = 0;
      unsigned i;
      memset(data, 0, sizeof(data));
      memcpy(data, "RIFF", 4);
      le32(data + 4, 70);
      memcpy(data + 8, "XWMAfmt ", 8);
      le32(data + 16, 18);
      data[20] = version ? 0x62 : 0x61;
      data[21] = 1;
      data[22] = 1;
      le32(data + 24, 48000);
      le32(data + 28, 16000);
      data[32] = 32;
      data[34] = 16;
      memcpy(data + 38, "data", 4);
      le32(data + 42, 32);
      if (!version)
      {
         bits(p, &at, 4, 8);
         bits(p, &at, 4, 2);
         bits(p, &at, 14, 0);
         bits(p, &at, 3, 0);
         bits(p, &at, 3, 0);
         bits(p, &at, 3, 0);
         bits(p, &at, 1, 0);
      }
      else
      {
         unsigned frame;
         at = 0;
         bits(p, &at, 6, 0);
         bits(p, &at, 9, 0);
         for (frame = 0; frame < 2; frame++)
         {
            unsigned end = at + 32;
            bits(p, &at, 9, 32);
            bits(p, &at, 1, 1);
            bits(p, &at, 1, 0);
            while (at < end - 1)
               bits(p, &at, 1, 0);
            bits(p, &at, 1, frame == 0);
         }
      }
      context = audio_transfer_new(AUDIO_TYPE_WMA);
      audio_transfer_set_buffer_ptr(context, AUDIO_TYPE_WMA, data, sizeof(data));
      check(audio_transfer_start(context, AUDIO_TYPE_WMA), "implicit xWMA setup");
      check(audio_transfer_read_f32(context, AUDIO_TYPE_WMA, output, 2048, &got) ==
                    AUDIO_PROCESS_NEXT &&
                got == 2048,
            "implicit xWMA packet framing");
      for (i = 0; i < 2048; i++)
         check(output[i] == 0, "implicit xWMA PCM");
      audio_transfer_free(context, AUDIO_TYPE_WMA);
   }
}
static void truncated_object_test(void)
{
   uint8_t data[512];
   void *context;
   size_t got = 0;
   asf(data, 1);
   le32(data + 256, 130);
   context = audio_transfer_new(AUDIO_TYPE_WMA);
   audio_transfer_set_buffer_ptr(context, AUDIO_TYPE_WMA, data, 370);
   check(audio_transfer_start(context, AUDIO_TYPE_WMA), "truncated object header");
   check(audio_transfer_read_f32(context, AUDIO_TYPE_WMA, output, 73, &got) ==
                 AUDIO_PROCESS_ERROR &&
             !got,
         "incomplete object is not clean EOF");
   audio_transfer_free(context, AUDIO_TYPE_WMA);
}
static void float_matrix_test(void)
{
   static uint8_t data[80+4096];
   float matrix[8][2];
   int32_t fixed[8][2];
   void *context;
   size_t size=xma_wave(data,1);
   context=audio_transfer_new(AUDIO_TYPE_WMA);
   audio_transfer_set_buffer_ptr(context,AUDIO_TYPE_WMA,data,size);
   check(audio_transfer_start(context,AUDIO_TYPE_WMA),"mono XMA header for float matrix");
   check(audio_transfer_stereo_matrix(context,AUDIO_TYPE_WMA,fixed) &&
         audio_transfer_stereo_matrix_f32(context,AUDIO_TYPE_WMA,matrix),"float speaker matrix");
   check(fixed[0][0]==23170 && matrix[0][0]==0.7071067811865475244f &&
         matrix[0][1]==matrix[0][0] && matrix[0][0]!=fixed[0][0]/32768.0f,
         "float downmix avoids Q15 coefficient quantization");
   audio_transfer_free(context,AUDIO_TYPE_WMA);
}
static void xma_test(void)
{
   static uint8_t data[80+4096];
   void *context;
   size_t size=xma_wave(data,3), got=0, i;
   int32_t matrix[8][2];
   unsigned channels=0, rate=0;
   check(audio_decode_get_type("test.xma")==AUDIO_TYPE_WMA,"XMA extension");
   context=audio_transfer_new(AUDIO_TYPE_WMA);
   audio_transfer_set_buffer_ptr(context,AUDIO_TYPE_WMA,data,size);
   audio_transfer_set_avail(context,AUDIO_TYPE_WMA,80+2048);
   check(audio_transfer_start(context,AUDIO_TYPE_WMA),"XMA2 header");
   check(audio_transfer_info(context,AUDIO_TYPE_WMA,&channels,&rate,NULL) &&
         channels==3 && rate==48000,"XMA2 native format");
   check(audio_transfer_stereo_matrix(context,AUDIO_TYPE_WMA,matrix) &&
         matrix[2][0]==0 && matrix[2][1]==0,"LFE is not centre");
   check(audio_transfer_read_f32(context,AUDIO_TYPE_WMA,output,512,&got)==AUDIO_PROCESS_NEXT && !got,
         "multistream waits for other stream");
   audio_transfer_set_avail(context,AUDIO_TYPE_WMA,size);
   check(audio_transfer_read_f32(context,AUDIO_TYPE_WMA,output,512,&got)!=AUDIO_PROCESS_ERROR && got==512,
         "XMA2 priming and overlap tail");
   for(i=0;i<got*channels;i++) check(output[i]==0,"XMA2 silent PCM");
   check(audio_transfer_read_f32(context,AUDIO_TYPE_WMA,output,1,&got)==AUDIO_PROCESS_END && !got,
         "XMA2 exact EOF");
   check(audio_transfer_seek(context,AUDIO_TYPE_WMA,0),"XMA2 rewind");
   check(audio_transfer_read_f32(context,AUDIO_TYPE_WMA,output,512,&got)!=AUDIO_PROCESS_ERROR && got==512,
         "XMA2 reset overlap");
   mixer_test(data,size);
   audio_transfer_free(context,AUDIO_TYPE_WMA);
}
int main(void)
{
   uint8_t data[512];
   size_t size;
   float_matrix_test();
   xma_test();
   expected();
   size = wave(data);
   transfer_test(data, size, 56);
   mixer_test(data, size);
   data[42] = 13;
   transfer_test(data, size, 56);
   size = asf(data, 0);
   transfer_test(data, size, 290);
   mixer_test(data, size);
   implicit_xwma_test();
   truncated_object_test();
   size = asf(data, 1);
   transfer_test(data, size, 370);
   mixer_test(data, size);
   puts("PASS WMA streaming transfer, fragment refill, quantization and mixer");
   return 0;
}
#else
int main(void)
{
   if (audio_transfer_new(AUDIO_TYPE_WMA) || audio_mixer_load_wma(NULL, 0))
      return 1;
   puts("PASS WMA disabled build");
   return 0;
}
#endif

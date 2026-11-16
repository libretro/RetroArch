/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include "../nvenc_frontend/core.c"

static uint64_t accepted;
static size_t clock_audio(const int16_t *data,size_t count,unsigned n,uint32_t mask)
{
   assert(data && count <= 3204 && n == channels && mask == layout);
   accepted += count;
   return count;
}
static size_t clock_float(const float *data,size_t count,unsigned n,uint32_t mask)
{
   size_t i;
   assert(data && count <= 3204 && n == channels && mask == layout);
   for (i = 0; i < count*n; i++)
      assert(data[i] >= -.2f && data[i] <= .2f);
   accepted += count;
   return count;
}
static void clock_video(const void *data,unsigned width,unsigned height,size_t pitch)
{
   assert(data && width == WIDTH && height == HEIGHT && pitch == WIDTH*sizeof(uint32_t));
}
static void check_rate(double rate,uint64_t numerator,unsigned denominator,bool use_float)
{
   struct retro_system_av_info info;
   unsigned i;
   sample_rate = rate;
   frames = 0;
   accepted = 0;
   channels = 8;
   layout = 0x63f;
   floating = use_float;
   multi.batch_int16 = clock_audio;
   multi.batch_float = clock_float;
   video = clock_video;
   retro_get_system_av_info(&info);
   assert(info.timing.sample_rate == rate);
   for (i = 0; i < 144; i++)
      retro_run();
   assert(accepted == 144*numerator*1001/(60000*denominator));
}
int main(void)
{
   check_rate(48000,48000,1,false);
   check_rate(32040.5,64081,2,false);
   check_rate(192000,192000,1,false);
   check_rate(32040.5,64081,2,true);
   puts("PASS native recording fixture preserves fractional clocks and high-rate eight-channel capacity");
   return 0;
}

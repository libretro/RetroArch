/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error Acceptance requires assertions
#endif
#include <assert.h>
#include <stdio.h>
#include "../../../libretro-common/audio/resampler/drivers/sinc_resampler_int16.c"

int main(void)
{
   static const unsigned rates_twice[] = {16000, 64081, 88201, 384000};
   rarch_sinc_resampler_int16_t clock;
   unsigned rate, i;
   for (rate = 0; rate < sizeof(rates_twice) / sizeof(rates_twice[0]); rate++)
   {
      double ratio = 96000.0 / rates_twice[rate];
      double step = 16777216.0 / ratio, carry = 0;
      uint64_t actual = 0, count = 1536 * (uint64_t)1012500;
      long double ideal, error;
      memset(&clock, 0, sizeof(clock));
      for (i = 0; i < 1012500; i++)
      {
         clock.time = 0;
         assert(sinc_i16_advance_aligned(&clock, 1536, step, &carry));
         actual += (uint64_t)floor(step) * 1536 + clock.time;
      }
      ideal = (long double)count * 16777216 * rates_twice[rate] / 96000;
      error = (long double)actual - ideal;
      assert(error > -64 && error < 64);
   }
   puts("PASS aligned clock: nine-hour sample clocks within 64/16777216 source frames");
   return 0;
}

#ifndef AUDIO_UTILS_H
#define AUDIO_UTILS_H

#include <stdint.h>

static inline void audio_convert_s16_to_float(float *out,
      const int16_t *in, unsigned samples)
{
   for (unsigned i = 0; i < samples; i++)
      out[i] = (float)in[i] / 0x8000; 
}

static inline void audio_convert_float_to_s16(int16_t *out,
      const float *in, unsigned samples)
{
   for (unsigned i = 0; i < samples; i++)
   {
      int32_t val = in[i] * 0x8000;
      out[i] = (val > 0x7FFF) ? 0x7FFF : (val < -0x8000 ? -0x8000 : (int16_t)val);
   }
}

#endif


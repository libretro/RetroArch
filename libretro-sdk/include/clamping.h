#ifndef _LIBRETRO_SDK_CLAMPING_H
#define _LIBRETRO_SDK_CLAMPING_H

#include <stdint.h>

static inline float clamp_float(float val, float lower, float upper)
{
   if (val < lower)
      return lower;
   if (val > upper)
      return upper;
   return val;
}

static inline uint8_t clamp_8bit(int val)
{
   if (val > 255)
      return 255;
   if (val < 0)
      return 0;
   return (uint8_t)val;
}

#endif

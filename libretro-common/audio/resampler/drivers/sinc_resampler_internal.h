#ifndef SINC_RESAMPLER_INTERNAL_H
#define SINC_RESAMPLER_INTERNAL_H

#include <stdint.h>
#include <string.h>
#include <retro_inline.h>

static INLINE int sinc_resampler_ratio_valid(double ratio,
      unsigned phase_bits, unsigned subphase_bits)
{
   uint64_t bits;
   unsigned phases = 1u << (phase_bits + subphase_bits);
   /* Integer classification survives the float driver's fast-math mode. */
   memcpy(&bits, &ratio, sizeof(bits));
   if (!bits || bits >= UINT64_C(0x7ff0000000000000))
      return 0;
   /* Leave room for the residual phase when advancing the 32-bit clock. */
   return ratio <= phases &&
      ratio >= (double)phases / (UINT32_MAX - phases);
}

#endif

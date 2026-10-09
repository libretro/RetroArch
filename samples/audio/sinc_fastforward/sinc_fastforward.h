#ifndef SINC_FASTFORWARD_H
#define SINC_FASTFORWARD_H

#include <stdio.h>
#include <stddef.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SINC_FF_IN  65536
#define SINC_FF_OUT (SINC_FF_IN * 5)

extern unsigned sinc_ff_failures;
#define SINC_FF_CHECK(x) do { if (!(x)) { printf("FAIL %s:%d: %s\n", \
   __FILE__, __LINE__, #x); sinc_ff_failures++; } } while (0)

/* Allocations inside processing and reset calls, under the GNU ld wraps. */
#ifdef SINC_TRACK_ALLOCATIONS
extern unsigned long sinc_ff_allocations;
extern unsigned long sinc_ff_mark, sinc_ff_guarded;
#define SINC_FF_GUARD_BEGIN() (sinc_ff_mark = sinc_ff_allocations)
#define SINC_FF_GUARD_END() \
   (sinc_ff_guarded += sinc_ff_allocations - sinc_ff_mark)
#define SINC_FF_GUARD_CALLS() sinc_ff_guarded
#else
#define SINC_FF_GUARD_BEGIN() ((void)0)
#define SINC_FF_GUARD_END() ((void)0)
#define SINC_FF_GUARD_CALLS() 0ul
#endif

void sinc_ff_tone_f(float *buf, size_t frames, double f, double amp);
void sinc_ff_noise_f(float *buf, size_t frames, unsigned seed);
/* Over the middle of the stream, clear of the filters' start-up. */
double sinc_ff_rms_f(const float *buf, size_t frames);

/* The integer driver's half, in the other translation unit. */
size_t sinc_ff_int16_stream(unsigned quality, const float *in, size_t frames,
      float *out, const double *ratios, size_t nratios, size_t chunk);
void sinc_ff_int16_tests(void);

#endif

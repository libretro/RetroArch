#ifndef SINC_REF_FLOAT_H
#define SINC_REF_FLOAT_H

#include <stddef.h>

struct resampler_data_float
{
   const float *data_in;
   float       *data_out;
   size_t       input_frames;
   size_t       output_frames;
   double       ratio;
};

void *sinc_ref_float_init(double bandwidth_mod, int quality);
void  sinc_ref_float_process(void *re, struct resampler_data_float *data);
void  sinc_ref_float_free(void *re);

#endif

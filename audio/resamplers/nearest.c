#include "resampler.h"
#include "../../libretro.h"
#include "../../performance.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
 
#if !defined(RESAMPLER_TEST) && defined(RARCH_INTERNAL)
#include "../../general.h"
#else
#define RARCH_LOG(...) fprintf(stderr, __VA_ARGS__)
#endif
 
typedef struct rarch_nearest_resampler
{
   float fraction;
} rarch_nearest_resampler_t;
 
typedef struct audio_frame_float
{
   float l;
   float r;
} audio_frame_float_t;
 
static void resampler_nearest_process(void *re_,
      struct resampler_data *data)
{
   rarch_nearest_resampler_t *re = (rarch_nearest_resampler_t*)re_;
 
   audio_frame_float_t *inp = (audio_frame_float_t*)data->data_in;
   audio_frame_float_t *inp_max = inp + data->input_frames;
   audio_frame_float_t *outp = (audio_frame_float_t*)data->data_out;
   float ratio = ratio = 1.0/data->ratio;
 
   while(inp != inp_max)
   {
      while(re->fraction > 1)
      {
         *outp++=*inp;
         re->fraction-=ratio;
      }
      re->fraction++;
      inp++;      
   }
   
   data->output_frames = (outp - (audio_frame_float_t*)data->data_out);
}
 
static void resampler_nearest_free(void *re_)
{
   rarch_nearest_resampler_t *re = (rarch_nearest_resampler_t*)re_;
   if (re)
      free(re);
}
 
static void *resampler_nearest_init(double bandwidth_mod)
{
   rarch_nearest_resampler_t *re = (rarch_nearest_resampler_t*)
      calloc(1, sizeof(rarch_nearest_resampler_t));
   if (!re)
      return NULL;
   
   re->fraction = 0;
   
   RARCH_LOG("\nNearest resampler : \n");
 
   return re;
}
 
rarch_resampler_t nearest_resampler = {
   resampler_nearest_init,
   resampler_nearest_process,
   resampler_nearest_free,
   "nearest",
};

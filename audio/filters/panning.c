#include "dspfilter.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct panning_data
{
   float left[2];
   float right[2];
};

static void panning_free(void *data)
{
   free(data);
}

static void panning_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   unsigned i;
   struct panning_data *pan = (struct panning_data*)data;

   output->samples = input->samples;
   output->frames  = input->frames;

   float *out = output->samples;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      float left = out[0];
      float right = out[1];
      out[0] = left * pan->left[0]  + right * pan->left[1];
      out[1] = left * pan->right[0] + right * pan->right[1];
   }
}

static void *panning_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   struct panning_data *pan = (struct panning_data*)calloc(1, sizeof(*pan));
   if (!pan)
      return NULL;

   float *left = NULL, *right = NULL;
   unsigned num_left = 0, num_right = 0;

   static const float default_left[] = { 1.0f, 0.0f };
   static const float default_right[] = { 0.0f, 1.0f };

   config->get_float_array(userdata, "left_mix", &left, &num_left, default_left, 2);
   config->get_float_array(userdata, "right_mix", &right, &num_right, default_right, 2);

   if (num_left == 2)
      memcpy(pan->left, left, sizeof(pan->left));
   if (num_right == 2)
      memcpy(pan->right, right, sizeof(pan->right));

   config->free(left);
   config->free(right);

   return pan;
}

static const struct dspfilter_implementation panning = {
   panning_init,
   panning_process,
   panning_free,

   DSPFILTER_API_VERSION,
   "Panning",
   "panning",
};

const struct dspfilter_implementation *dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   (void)mask;
   return &panning;
}


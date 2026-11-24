/* Fixed-point variant of Shay Green's SNES NTSC kernel generator.
 * LGPL-2.1-or-later; see snes_ntsc_impl.h for the license notice.
 * Q32.32 throughout; setup runs once, never in the blitter. */
#include "snes_ntsc_fixed_math.h"
typedef snes_fixed_t snes_ntsc_scalar_t;
#define rgb_offset (256 * SNES_F_ONE + SNES_F_ONE/2)
typedef struct init_t
{
   snes_fixed_t to_rgb[18], to_float[32];
   snes_fixed_t artifacts, fringing;
   snes_fixed_t kernel[rescale_out * kernel_size * 2];
} init_t;
static void init_filters(init_t *impl, const snes_ntsc_setup_t *setup)
{
   snes_fixed_t kernels[kernel_size * 2];
   snes_fixed_t rolloff = SNES_F_ONE + snes_f_mul(snes_f_setting(setup->sharpness), SNES_F(32,1000));
   snes_fixed_t power = snes_f_pow(rolloff, 32 * SNES_F_ONE);
   snes_fixed_t angle = snes_f_setting(setup->resolution) + SNES_F_ONE;
   snes_fixed_t sum = 0, cutoff, weight = SNES_F_ONE;
   snes_fixed_t *out = impl->kernel;
   int i, phase;
   angle = snes_f_mul(SNES_F_PI/160, snes_f_mul(angle,angle) + SNES_F_ONE);
   kernels[kernel_size * 3/2] = 32 * SNES_F_ONE;
   for (i = 0; i < kernel_size; i++)
   {
      int x = i - kernel_half;
      if (x || power > SNES_F(1056,1000) || power < SNES_F(981,1000))
      {
         snes_fixed_t a = x * angle;
         snes_fixed_t rc = snes_f_mul(rolloff, snes_f_cos(a));
         snes_fixed_t num = SNES_F_ONE - rc - snes_f_mul(power, snes_f_cos(32*a))
            + snes_f_mul(snes_f_mul(power,rolloff), snes_f_cos(31*a));
         snes_fixed_t den = SNES_F_ONE - 2*rc + snes_f_mul(rolloff,rolloff);
         kernels[kernel_size+i] = snes_f_div(num,den) - SNES_F_ONE/2;
      }
   }
   for (i = 0; i < kernel_size; i++)
   {
      snes_fixed_t x = SNES_F_PI * i / 16;
      snes_fixed_t blackman = SNES_F(42,100) - snes_f_cos(x)/2
         + snes_f_mul(SNES_F(8,100),snes_f_cos(2*x));
      kernels[kernel_size+i] = snes_f_mul(kernels[kernel_size+i],blackman);
      sum += kernels[kernel_size+i];
   }
   sum = snes_f_div(SNES_F_ONE,sum);
   for (i = 0; i < kernel_size; i++)
      kernels[kernel_size+i] = snes_f_mul(kernels[kernel_size+i],sum);
   cutoff = snes_f_setting(setup->bleed);
   if (cutoff < 0)
   {
      for (i = 0; i < 3; i++) cutoff = snes_f_mul(cutoff,cutoff);
      cutoff = -snes_f_mul(cutoff,SNES_F(3000,65));
   }
   cutoff = -SNES_F_ONE/32 + snes_f_mul(SNES_F(65,3200),cutoff);
   for (i = -kernel_half; i <= kernel_half; i++)
      kernels[kernel_half+i] = snes_f_exp(i*i*cutoff);
   for (phase = 0; phase < 2; phase++)
   {
      sum = 0;
      for (i = phase; i < kernel_size; i += 2) sum += kernels[i];
      sum = snes_f_div(SNES_F_ONE,sum);
      for (i = phase; i < kernel_size; i += 2) kernels[i] = snes_f_mul(kernels[i],sum);
   }
   for (phase = 0; phase < rescale_out; phase++)
   {
      snes_fixed_t remain = 0;
      weight -= SNES_F_ONE/rescale_in;
      for (i = 0; i < kernel_size*2; i++)
      {
         snes_fixed_t m = snes_f_mul(kernels[i],weight);
         *out++ = m + remain;
         remain = kernels[i] - m;
      }
   }
}
static void init(init_t *impl, const snes_ntsc_setup_t *setup)
{
   static const snes_fixed_t decoder[6] = {
      SNES_F(956,1000), SNES_F(621,1000), -SNES_F(272,1000),
      -SNES_F(647,1000), -SNES_F(1105,1000), SNES_F(1702,1000)
   };
   snes_fixed_t brightness = snes_f_setting(setup->brightness)*64 + rgb_offset;
   snes_fixed_t contrast = snes_f_setting(setup->contrast)*64 + 128*SNES_F_ONE;
   snes_fixed_t gamma = SNES_F(11333,10000) - snes_f_idiv(snes_f_setting(setup->gamma),2);
   snes_fixed_t hue = snes_f_mul(snes_f_setting(setup->hue), SNES_F_PI);
   snes_fixed_t sat = snes_f_setting(setup->saturation) + SNES_F_ONE;
   snes_fixed_t s, c;
   int i, burst;
   impl->artifacts = snes_f_setting(setup->artifacts);
   if (impl->artifacts > 0) impl->artifacts = snes_f_idiv(impl->artifacts,2);
   impl->artifacts += SNES_F_ONE;
   impl->fringing = snes_f_setting(setup->fringing) + SNES_F_ONE;
   init_filters(impl,setup);
   for (i = 0; i < gamma_size; i++)
      impl->to_float[i] = snes_f_mul(snes_f_pow(SNES_F_ONE*i/31,gamma),contrast) + brightness;
   if (setup->decoder_matrix) hue += SNES_F_PI/12;
   s = snes_f_mul(snes_f_sin(hue),sat);
   c = snes_f_mul(snes_f_cos(hue),sat);
   for (burst = 0; burst < burst_count; burst++)
   {
      snes_fixed_t t;
      for (i = 0; i < 3; i++)
      {
         snes_fixed_t di = setup->decoder_matrix ? snes_f_decoder(setup->decoder_matrix[i*2]) : decoder[i*2];
         snes_fixed_t dq = setup->decoder_matrix ? snes_f_decoder(setup->decoder_matrix[i*2+1]) : decoder[i*2+1];
         impl->to_rgb[burst*6+i*2] = snes_f_mul(di,c) - snes_f_mul(dq,s);
         impl->to_rgb[burst*6+i*2+1] = snes_f_mul(di,s) + snes_f_mul(dq,c);
      }
      t = -snes_f_idiv(s,2) - snes_f_mul(c,SNES_F(866025,1000000));
      c = snes_f_mul(s,SNES_F(866025,1000000)) - snes_f_idiv(c,2);
      s = t;
   }
}
#define RGB_TO_YIQ(r,g,b,y,i) (\
   (y = snes_f_mul(r,SNES_F(299,1000)) + snes_f_mul(g,SNES_F(587,1000)) + snes_f_mul(b,SNES_F(114,1000))),\
   (i = snes_f_mul(r,SNES_F(596,1000)) - snes_f_mul(g,SNES_F(275,1000)) - snes_f_mul(b,SNES_F(321,1000))),\
   (snes_f_mul(r,SNES_F(212,1000)) - snes_f_mul(g,SNES_F(523,1000)) + snes_f_mul(b,SNES_F(311,1000))))
#define YIQ_TO_RGB(y,i,q,m,type,r,g) (\
   r = snes_f_int(y + snes_f_mul(m[0],i) + snes_f_mul(m[1],q)),\
   g = snes_f_int(y + snes_f_mul(m[2],i) + snes_f_mul(m[3],q)),\
   snes_f_int(y + snes_f_mul(m[4],i) + snes_f_mul(m[5],q)))
/* Explicit unsigned packing preserves the original modulo arithmetic without
 * shifting negative signed integers. Kernel entries have a defined width. */
#define PACK_RGB(r,g,b) (((uint64_t)(r)<<21) | ((uint64_t)(g)<<11) | ((uint64_t)(b)<<1))
enum { rgb_kernel_size = burst_size / alignment_count };
enum { rgb_bias = rgb_unit * 2 * snes_ntsc_rgb_builder };
typedef struct pixel_info_t
{
   int offset;
   int negate;
   snes_fixed_t kernel[4];
} pixel_info_t;
#define PIXEL_OFFSET_(ntsc,scaled) \
   (kernel_size/2 + ntsc + (scaled != 0) + (rescale_out-scaled)%rescale_out + kernel_size*2*scaled)
#define PIXEL_OFFSET(ntsc,scaled) \
   PIXEL_OFFSET_(((ntsc)-(scaled)/rescale_out*rescale_in), (((scaled)+rescale_out*10)%rescale_out)), \
   (1 - (((ntsc)+100)&2))
extern const pixel_info_t retroarch_snes_ntsc_pixels[alignment_count];
static void gen_kernel(init_t *impl, snes_fixed_t y, snes_fixed_t i,
      snes_fixed_t q, snes_ntsc_build_rgb_t *out)
{
   const snes_fixed_t *matrix = impl->to_rgb;
   int burst;
   y -= rgb_offset;
   for (burst = 0; burst < burst_count; burst++)
   {
      int alignment;
      snes_fixed_t t;
      for (alignment = 0; alignment < alignment_count; alignment++)
      {
         const pixel_info_t *p = &retroarch_snes_ntsc_pixels[alignment];
         snes_fixed_t yy = snes_f_mul(y,impl->fringing)*p->negate;
         snes_fixed_t ic0 = snes_f_mul(i+yy,p->kernel[0]);
         snes_fixed_t qc1 = snes_f_mul(q+yy,p->kernel[1]);
         snes_fixed_t ic2 = snes_f_mul(i-yy,p->kernel[2]);
         snes_fixed_t qc3 = snes_f_mul(q-yy,p->kernel[3]);
         snes_fixed_t factor = impl->artifacts*p->negate;
         snes_fixed_t ii = snes_f_mul(i,factor), qq = snes_f_mul(q,factor);
         snes_fixed_t yc0 = snes_f_mul(y+ii,p->kernel[0]);
         snes_fixed_t yc2 = snes_f_mul(y-ii,p->kernel[2]);
         snes_fixed_t yc1 = snes_f_mul(y+qq,p->kernel[1]);
         snes_fixed_t yc3 = snes_f_mul(y-qq,p->kernel[3]);
         const snes_fixed_t *k = &impl->kernel[p->offset];
         int n;
         for (n = rgb_kernel_size; n; n--)
         {
            snes_fixed_t fi = snes_f_mul(k[0],ic0) + snes_f_mul(k[2],ic2);
            snes_fixed_t fq = snes_f_mul(k[1],qc1) + snes_f_mul(k[3],qc3);
            snes_fixed_t fy = snes_f_mul(k[kernel_size],yc0) + snes_f_mul(k[kernel_size+1],yc1)
               + snes_f_mul(k[kernel_size+2],yc2) + snes_f_mul(k[kernel_size+3],yc3) + rgb_offset;
            int r, g, b = YIQ_TO_RGB(fy,fi,fq,matrix,int,r,g);
            *out++ = PACK_RGB(r,g,b) - rgb_bias;
            if (k < &impl->kernel[kernel_size*2*(rescale_out-1)]) k += kernel_size*2-1;
            else k -= kernel_size*2*(rescale_out-1)+2;
         }
      }
      matrix += 6;
      t = -snes_f_idiv(i,2) + snes_f_mul(q,SNES_F(866025,1000000));
      q = -snes_f_mul(i,SNES_F(866025,1000000)) - snes_f_idiv(q,2);
      i = t;
   }
}

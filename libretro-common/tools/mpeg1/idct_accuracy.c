/* IDCT accuracy against a double-precision reference, in the style of
 * IEEE 1180-1990.
 *
 * Comparing our output against another decoder only says the two differ; it
 * does not say which is wrong. This measures our IDCT on its own terms:
 * random coefficient blocks in, double-precision floating point IDCT as the
 * reference, and the error statistics 1180 defines.
 *
 * Coefficient ranges are chosen to look like real intra blocks -- a large DC
 * around the 1024 predictor and small AC -- as well as 1180's own uniform
 * ranges, because an IDCT can be accurate on one and not the other.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

#include "../../formats/mpeg1/rmpeg1_video.c"

static unsigned long rs = 1;
static long rnd(long lo, long hi)
{ rs = rs*6364136223846793005ULL + 1442695040888963407ULL;
  return lo + (long)((rs>>33) % (unsigned long)(hi-lo+1)); }

static void ref_idct(const int16_t *in, double *out)
{
   int u,v,x,y;
   static double c[8][8]; static int init=0;
   if(!init){ for(u=0;u<8;u++) for(x=0;x<8;x++)
      c[u][x]=(u?0.5:0.5/sqrt(2.0))*cos((2*x+1)*u*M_PI/16.0); init=1; }
   for(y=0;y<8;y++) for(x=0;x<8;x++){
      double s=0;
      for(v=0;v<8;v++) for(u=0;u<8;u++) s += c[v][y]*c[u][x]*(double)in[v*8+u];
      out[y*8+x]=s;
   }
}

static int run(const char *name, int dc_lo, int dc_hi, int ac_lo, int ac_hi, int n)
{
   int i,k, peak=0; double sume=0, sumsq=0;
   double pme[64], me_n[64]; int cnt[64];
   int16_t blk[64], save[64]; uint8_t got[64]; double refv[64];
   memset(pme,0,sizeof(pme)); memset(me_n,0,sizeof(me_n)); memset(cnt,0,sizeof(cnt));

   for(i=0;i<n;i++){
      for(k=0;k<64;k++) blk[k]=(int16_t)(k? rnd(ac_lo,ac_hi) : rnd(dc_lo,dc_hi));
      memcpy(save,blk,sizeof(blk));
      ref_idct(save,refv);
      idct_block(blk,got,8);
      for(k=0;k<64;k++){
         double r=refv[k]; int ri;
         if(r<0) r=0; if(r>255) r=255;
         /* The reference accumulates 64 double products, so a value that is
          * mathematically an exact .5 tie lands a few ulp below it. Without
          * a nudge, every tie reads as a +1 bias from us -- which is how the
          * DC-only case first appeared to fail with a suspiciously exact
          * 1-in-8 error rate. */
         ri=(int)floor(r+0.5+1e-9);
         { int d=(int)got[k]-ri; int ad=d<0?-d:d;
           if(ad>peak) peak=ad;
           sume+=d; sumsq+=(double)d*d;
           pme[k]+=d; cnt[k]++; }
      }
   }
   { double me=sume/(n*64.0), mse=sumsq/(n*64.0); double worst_pme=0;
     for(k=0;k<64;k++){ double v=fabs(pme[k]/cnt[k]); if(v>worst_pme) worst_pme=v; }
     printf("  %-26s peak=%d  mse=%.5f  me=%.6f  worst_pme=%.5f  %s\n",
            name, peak, mse, me, worst_pme,
            (peak<=1 && mse<=0.06 && fabs(me)<=0.015 && worst_pme<=0.015) ? "OK" : "OUT OF SPEC");
     return (peak<=1 && mse<=0.06 && fabs(me)<=0.015 && worst_pme<=0.015)?0:1; }
}

int main(void)
{
   int bad=0;
   printf("IDCT accuracy vs double-precision reference (IEEE 1180 style)\n");
   bad += run("intra-like (DC~1024)", 900, 1150, -40, 40, 20000);
   bad += run("1180 range L=256",    -256, 255, -256, 255, 20000);
   bad += run("1180 range L=5",        -5,   5,   -5,   5, 20000);
   bad += run("1180 range L=300",    -300, 300, -300, 300, 20000);
   bad += run("DC only",              900, 1150,   0,   0, 5000);
   { int16_t z[64]; uint8_t o[64]; int k, allz=1;
     memset(z,0,sizeof(z)); idct_block(z,o,8);
     for(k=0;k<64;k++) if(o[k]!=0) allz=0;
     printf("  %-26s %s\n","all-zero input", allz?"OK (all zero out)":"FAIL"); if(!allz) bad++; }
   printf(bad?"RESULT: FAIL\n":"RESULT: PASS\n");
   return bad?1:0;
}

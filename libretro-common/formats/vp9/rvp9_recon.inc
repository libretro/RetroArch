/* rvp9_recon.inc -- pixel-path reconstruction layer of rvp9.
 *
 * Included by rvp9.c in four parts (RVP9_RECON_PART 1-4) at the exact
 * positions the code originally occupied, so the translation-unit
 * definition order -- and therefore GCC's order-sensitive inlining of
 * static functions -- is unchanged and the 8-bit build stays
 * instruction-identical to the pre-split decoder (verified by
 * normalized objdump comparison).  Parts:
 *   1: intra predictors + IHT/IWHT add stages
 *   2: intra edge assembly/dispatch, mode decode, block decode,
 *      partition recursion
 *   3: frame decode body (rvp9_decode_frame_impl)
 *   4: loop filter kernels + convolve/inter prediction + LF frame walk
 * A second, uint16 high-bit-depth instantiation of these parts follows
 * in a later stage. */

#if RVP9_RECON_PART == 1

/* ==================================================================== */
/* Inverse transforms, ported from vpx_dsp/inv_txfm.c (generic C).      */
/* ==================================================================== */
#define RVP9_DCT_CONST_BITS 14
static INLINE int rvp9_round_shift(int64_t in)
{
   return (int)((in + (1 << (RVP9_DCT_CONST_BITS - 1))) >> RVP9_DCT_CONST_BITS);
}
static INLINE int rvp9_clamp(int v, int lo, int hi)
{
   return v < lo ? lo : v > hi ? hi : v;
}

static INLINE RVP9_PIXEL rvp9_clip8(int v)
{
   return (RVP9_PIXEL)(v < 0 ? 0 : v > RVP9_PXMAX ? RVP9_PXMAX : v);
}
/* cospi/sinpi constants (vpx_dsp/txfm_common.h) */
#define c1_p64  16364
#define RVP9_WRAP(x) (x)
static const int64_t rvp9_cospi64[32] = {
   16384, 16364, 16305, 16207, 16069, 15893, 15679, 15426,
   15137, 14811, 14449, 14053, 13623, 13160, 12665, 12140,
   11585, 11003, 10394,  9760,  9102,  8423,  7723,  7005,
    6270,  5520,  4756,  3981,  3196,  2404,  1606,   804
};
static const int64_t rvp9_sinpi[5] = { 0, 5283, 9929, 13377, 15212 };


static void rvp9_idct4(const rvp9_tran *input, rvp9_tran *output)
{
   RVP9_TRAN_STEP step[4];
   int64_t temp1, temp2;
   temp1 = ((int)input[0] + input[2]) * rvp9_cospi64[16];
   temp2 = ((int)input[0] - input[2]) * rvp9_cospi64[16];
   step[0] = (RVP9_TRAN_STEP)rvp9_round_shift(temp1);
   step[1] = (RVP9_TRAN_STEP)rvp9_round_shift(temp2);
   temp1 = (int)input[1] * rvp9_cospi64[24] - (int)input[3] * rvp9_cospi64[8];
   temp2 = (int)input[1] * rvp9_cospi64[8]  + (int)input[3] * rvp9_cospi64[24];
   step[2] = (RVP9_TRAN_STEP)rvp9_round_shift(temp1);
   step[3] = (RVP9_TRAN_STEP)rvp9_round_shift(temp2);
   output[0] = step[0] + step[3];
   output[1] = step[1] + step[2];
   output[2] = step[1] - step[2];
   output[3] = step[0] - step[3];
}

static void rvp9_iadst4(const rvp9_tran *input, rvp9_tran *output)
{
   int64_t s0, s1, s2, s3, s4, s5, s6, s7;
   int64_t x0 = input[0], x1 = input[1], x2 = input[2], x3 = input[3];
   if (!(x0 | x1 | x2 | x3))
   {
      output[0] = output[1] = output[2] = output[3] = 0;
      return;
   }
   s0 = rvp9_sinpi[1] * x0;
   s1 = rvp9_sinpi[2] * x0;
   s2 = rvp9_sinpi[3] * x1;
   s3 = rvp9_sinpi[4] * x2;
   s4 = rvp9_sinpi[1] * x2;
   s5 = rvp9_sinpi[2] * x3;
   s6 = rvp9_sinpi[4] * x3;
   s7 = (int)(x0 - x2 + x3);
   x0 = s0 + s3 + s5;
   x1 = s1 - s4 - s6;
   x2 = rvp9_sinpi[3] * s7;
   x3 = s2;
   s0 = x0 + x3;
   s1 = x1 + x3;
   s2 = x2;
   s3 = x0 + x1 - x3;
   output[0] = rvp9_round_shift(s0);
   output[1] = rvp9_round_shift(s1);
   output[2] = rvp9_round_shift(s2);
   output[3] = rvp9_round_shift(s3);
}

static void rvp9_idct8(const rvp9_tran *input, rvp9_tran *output)
{
   RVP9_TRAN_STEP step1[8], step2[8];
   int64_t temp1, temp2;
   step1[0] = (RVP9_TRAN_STEP)input[0];
   step1[2] = (RVP9_TRAN_STEP)input[4];
   step1[1] = (RVP9_TRAN_STEP)input[2];
   step1[3] = (RVP9_TRAN_STEP)input[6];
   temp1 = (int)input[1] * rvp9_cospi64[28] - (int)input[7] * rvp9_cospi64[4];
   temp2 = (int)input[1] * rvp9_cospi64[4]  + (int)input[7] * rvp9_cospi64[28];
   step1[4] = (RVP9_TRAN_STEP)rvp9_round_shift(temp1);
   step1[7] = (RVP9_TRAN_STEP)rvp9_round_shift(temp2);
   temp1 = (int)input[5] * rvp9_cospi64[12] - (int)input[3] * rvp9_cospi64[20];
   temp2 = (int)input[5] * rvp9_cospi64[20] + (int)input[3] * rvp9_cospi64[12];
   step1[5] = (RVP9_TRAN_STEP)rvp9_round_shift(temp1);
   step1[6] = (RVP9_TRAN_STEP)rvp9_round_shift(temp2);

   /* stage 2: 4-point idct on evens */
   temp1 = (step1[0] + step1[2]) * rvp9_cospi64[16];
   temp2 = (step1[0] - step1[2]) * rvp9_cospi64[16];
   step2[0] = (RVP9_TRAN_STEP)rvp9_round_shift(temp1);
   step2[1] = (RVP9_TRAN_STEP)rvp9_round_shift(temp2);
   temp1 = step1[1] * rvp9_cospi64[24] - step1[3] * rvp9_cospi64[8];
   temp2 = step1[1] * rvp9_cospi64[8]  + step1[3] * rvp9_cospi64[24];
   step2[2] = (RVP9_TRAN_STEP)rvp9_round_shift(temp1);
   step2[3] = (RVP9_TRAN_STEP)rvp9_round_shift(temp2);
   step2[4] = (RVP9_TRAN_STEP)(step1[4] + step1[5]);
   step2[5] = (RVP9_TRAN_STEP)(step1[4] - step1[5]);
   step2[6] = (RVP9_TRAN_STEP)(-step1[6] + step1[7]);
   step2[7] = (RVP9_TRAN_STEP)(step1[6] + step1[7]);

   /* stage 3 */
   step1[0] = (RVP9_TRAN_STEP)(step2[0] + step2[3]);
   step1[1] = (RVP9_TRAN_STEP)(step2[1] + step2[2]);
   step1[2] = (RVP9_TRAN_STEP)(step2[1] - step2[2]);
   step1[3] = (RVP9_TRAN_STEP)(step2[0] - step2[3]);
   step1[4] = step2[4];
   temp1 = (step2[6] - step2[5]) * rvp9_cospi64[16];
   temp2 = (step2[5] + step2[6]) * rvp9_cospi64[16];
   step1[5] = (RVP9_TRAN_STEP)rvp9_round_shift(temp1);
   step1[6] = (RVP9_TRAN_STEP)rvp9_round_shift(temp2);
   step1[7] = step2[7];

   output[0] = step1[0] + step1[7];
   output[1] = step1[1] + step1[6];
   output[2] = step1[2] + step1[5];
   output[3] = step1[3] + step1[4];
   output[4] = step1[3] - step1[4];
   output[5] = step1[2] - step1[5];
   output[6] = step1[1] - step1[6];
   output[7] = step1[0] - step1[7];
}

static void rvp9_iadst8(const rvp9_tran *input, rvp9_tran *output)
{
   int64_t s0,s1,s2,s3,s4,s5,s6,s7;
   int64_t x0 = input[7], x1 = input[0], x2 = input[5], x3 = input[2];
   int64_t x4 = input[3], x5 = input[4], x6 = input[1], x7 = input[6];
   if (!(x0|x1|x2|x3|x4|x5|x6|x7))
   {
      memset(output, 0, 8 * sizeof(*output));
      return;
   }
   /* stage 1 */
   s0 = (int)(rvp9_cospi64[2]  * x0 + rvp9_cospi64[30] * x1);
   s1 = (int)(rvp9_cospi64[30] * x0 - rvp9_cospi64[2]  * x1);
   s2 = (int)(rvp9_cospi64[10] * x2 + rvp9_cospi64[22] * x3);
   s3 = (int)(rvp9_cospi64[22] * x2 - rvp9_cospi64[10] * x3);
   s4 = (int)(rvp9_cospi64[18] * x4 + rvp9_cospi64[14] * x5);
   s5 = (int)(rvp9_cospi64[14] * x4 - rvp9_cospi64[18] * x5);
   s6 = (int)(rvp9_cospi64[26] * x6 + rvp9_cospi64[6]  * x7);
   s7 = (int)(rvp9_cospi64[6]  * x6 - rvp9_cospi64[26] * x7);
   x0 = rvp9_round_shift(s0 + s4);
   x1 = rvp9_round_shift(s1 + s5);
   x2 = rvp9_round_shift(s2 + s6);
   x3 = rvp9_round_shift(s3 + s7);
   x4 = rvp9_round_shift(s0 - s4);
   x5 = rvp9_round_shift(s1 - s5);
   x6 = rvp9_round_shift(s2 - s6);
   x7 = rvp9_round_shift(s3 - s7);
   /* stage 2 */
   s0 = x0; s1 = x1; s2 = x2; s3 = x3;
   s4 = (int)( rvp9_cospi64[8]  * x4 + rvp9_cospi64[24] * x5);
   s5 = (int)( rvp9_cospi64[24] * x4 - rvp9_cospi64[8]  * x5);
   s6 = (int)(-rvp9_cospi64[24] * x6 + rvp9_cospi64[8]  * x7);
   s7 = (int)( rvp9_cospi64[8]  * x6 + rvp9_cospi64[24] * x7);
   x0 = s0 + s2;
   x1 = s1 + s3;
   x2 = s0 - s2;
   x3 = s1 - s3;
   x4 = rvp9_round_shift(s4 + s6);
   x5 = rvp9_round_shift(s5 + s7);
   x6 = rvp9_round_shift(s4 - s6);
   x7 = rvp9_round_shift(s5 - s7);
   /* stage 3 */
   s2 = (int)(rvp9_cospi64[16] * (x2 + x3));
   s3 = (int)(rvp9_cospi64[16] * (x2 - x3));
   s6 = (int)(rvp9_cospi64[16] * (x6 + x7));
   s7 = (int)(rvp9_cospi64[16] * (x6 - x7));
   x2 = rvp9_round_shift(s2);
   x3 = rvp9_round_shift(s3);
   x6 = rvp9_round_shift(s6);
   x7 = rvp9_round_shift(s7);
   output[0] = x0;
   output[1] = -x4;
   output[2] = x6;
   output[3] = -x2;
   output[4] = x3;
   output[5] = -x7;
   output[6] = x5;
   output[7] = -x1;
}

static void rvp9_idct16(const rvp9_tran *input, rvp9_tran *output) {
  RVP9_TRAN_STEP step1[16], step2[16];
  int64_t temp1, temp2;

/* stage 1 */
  step1[0] = input[0 / 2];
  step1[1] = input[16 / 2];
  step1[2] = input[8 / 2];
  step1[3] = input[24 / 2];
  step1[4] = input[4 / 2];
  step1[5] = input[20 / 2];
  step1[6] = input[12 / 2];
  step1[7] = input[28 / 2];
  step1[8] = input[2 / 2];
  step1[9] = input[18 / 2];
  step1[10] = input[10 / 2];
  step1[11] = input[26 / 2];
  step1[12] = input[6 / 2];
  step1[13] = input[22 / 2];
  step1[14] = input[14 / 2];
  step1[15] = input[30 / 2];

/* stage 2 */
  step2[0] = step1[0];
  step2[1] = step1[1];
  step2[2] = step1[2];
  step2[3] = step1[3];
  step2[4] = step1[4];
  step2[5] = step1[5];
  step2[6] = step1[6];
  step2[7] = step1[7];

  temp1 = step1[8] * rvp9_cospi64[30] - step1[15] * rvp9_cospi64[2];
  temp2 = step1[8] * rvp9_cospi64[2] + step1[15] * rvp9_cospi64[30];
  step2[8] = RVP9_WRAP(rvp9_round_shift(temp1));
  step2[15] = RVP9_WRAP(rvp9_round_shift(temp2));

  temp1 = step1[9] * rvp9_cospi64[14] - step1[14] * rvp9_cospi64[18];
  temp2 = step1[9] * rvp9_cospi64[18] + step1[14] * rvp9_cospi64[14];
  step2[9] = RVP9_WRAP(rvp9_round_shift(temp1));
  step2[14] = RVP9_WRAP(rvp9_round_shift(temp2));

  temp1 = step1[10] * rvp9_cospi64[22] - step1[13] * rvp9_cospi64[10];
  temp2 = step1[10] * rvp9_cospi64[10] + step1[13] * rvp9_cospi64[22];
  step2[10] = RVP9_WRAP(rvp9_round_shift(temp1));
  step2[13] = RVP9_WRAP(rvp9_round_shift(temp2));

  temp1 = step1[11] * rvp9_cospi64[6] - step1[12] * rvp9_cospi64[26];
  temp2 = step1[11] * rvp9_cospi64[26] + step1[12] * rvp9_cospi64[6];
  step2[11] = RVP9_WRAP(rvp9_round_shift(temp1));
  step2[12] = RVP9_WRAP(rvp9_round_shift(temp2));

/* stage 3 */
  step1[0] = step2[0];
  step1[1] = step2[1];
  step1[2] = step2[2];
  step1[3] = step2[3];

  temp1 = step2[4] * rvp9_cospi64[28] - step2[7] * rvp9_cospi64[4];
  temp2 = step2[4] * rvp9_cospi64[4] + step2[7] * rvp9_cospi64[28];
  step1[4] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[7] = RVP9_WRAP(rvp9_round_shift(temp2));
  temp1 = step2[5] * rvp9_cospi64[12] - step2[6] * rvp9_cospi64[20];
  temp2 = step2[5] * rvp9_cospi64[20] + step2[6] * rvp9_cospi64[12];
  step1[5] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[6] = RVP9_WRAP(rvp9_round_shift(temp2));

  step1[8] = RVP9_WRAP(step2[8] + step2[9]);
  step1[9] = RVP9_WRAP(step2[8] - step2[9]);
  step1[10] = RVP9_WRAP(-step2[10] + step2[11]);
  step1[11] = RVP9_WRAP(step2[10] + step2[11]);
  step1[12] = RVP9_WRAP(step2[12] + step2[13]);
  step1[13] = RVP9_WRAP(step2[12] - step2[13]);
  step1[14] = RVP9_WRAP(-step2[14] + step2[15]);
  step1[15] = RVP9_WRAP(step2[14] + step2[15]);

/* stage 4 */
  temp1 = (step1[0] + step1[1]) * rvp9_cospi64[16];
  temp2 = (step1[0] - step1[1]) * rvp9_cospi64[16];
  step2[0] = RVP9_WRAP(rvp9_round_shift(temp1));
  step2[1] = RVP9_WRAP(rvp9_round_shift(temp2));
  temp1 = step1[2] * rvp9_cospi64[24] - step1[3] * rvp9_cospi64[8];
  temp2 = step1[2] * rvp9_cospi64[8] + step1[3] * rvp9_cospi64[24];
  step2[2] = RVP9_WRAP(rvp9_round_shift(temp1));
  step2[3] = RVP9_WRAP(rvp9_round_shift(temp2));
  step2[4] = RVP9_WRAP(step1[4] + step1[5]);
  step2[5] = RVP9_WRAP(step1[4] - step1[5]);
  step2[6] = RVP9_WRAP(-step1[6] + step1[7]);
  step2[7] = RVP9_WRAP(step1[6] + step1[7]);

  step2[8] = step1[8];
  step2[15] = step1[15];
  temp1 = -step1[9] * rvp9_cospi64[8] + step1[14] * rvp9_cospi64[24];
  temp2 = step1[9] * rvp9_cospi64[24] + step1[14] * rvp9_cospi64[8];
  step2[9] = RVP9_WRAP(rvp9_round_shift(temp1));
  step2[14] = RVP9_WRAP(rvp9_round_shift(temp2));
  temp1 = -step1[10] * rvp9_cospi64[24] - step1[13] * rvp9_cospi64[8];
  temp2 = -step1[10] * rvp9_cospi64[8] + step1[13] * rvp9_cospi64[24];
  step2[10] = RVP9_WRAP(rvp9_round_shift(temp1));
  step2[13] = RVP9_WRAP(rvp9_round_shift(temp2));
  step2[11] = step1[11];
  step2[12] = step1[12];

/* stage 5 */
  step1[0] = RVP9_WRAP(step2[0] + step2[3]);
  step1[1] = RVP9_WRAP(step2[1] + step2[2]);
  step1[2] = RVP9_WRAP(step2[1] - step2[2]);
  step1[3] = RVP9_WRAP(step2[0] - step2[3]);
  step1[4] = step2[4];
  temp1 = (step2[6] - step2[5]) * rvp9_cospi64[16];
  temp2 = (step2[5] + step2[6]) * rvp9_cospi64[16];
  step1[5] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[6] = RVP9_WRAP(rvp9_round_shift(temp2));
  step1[7] = step2[7];

  step1[8] = RVP9_WRAP(step2[8] + step2[11]);
  step1[9] = RVP9_WRAP(step2[9] + step2[10]);
  step1[10] = RVP9_WRAP(step2[9] - step2[10]);
  step1[11] = RVP9_WRAP(step2[8] - step2[11]);
  step1[12] = RVP9_WRAP(-step2[12] + step2[15]);
  step1[13] = RVP9_WRAP(-step2[13] + step2[14]);
  step1[14] = RVP9_WRAP(step2[13] + step2[14]);
  step1[15] = RVP9_WRAP(step2[12] + step2[15]);

/* stage 6 */
  step2[0] = RVP9_WRAP(step1[0] + step1[7]);
  step2[1] = RVP9_WRAP(step1[1] + step1[6]);
  step2[2] = RVP9_WRAP(step1[2] + step1[5]);
  step2[3] = RVP9_WRAP(step1[3] + step1[4]);
  step2[4] = RVP9_WRAP(step1[3] - step1[4]);
  step2[5] = RVP9_WRAP(step1[2] - step1[5]);
  step2[6] = RVP9_WRAP(step1[1] - step1[6]);
  step2[7] = RVP9_WRAP(step1[0] - step1[7]);
  step2[8] = step1[8];
  step2[9] = step1[9];
  temp1 = (-step1[10] + step1[13]) * rvp9_cospi64[16];
  temp2 = (step1[10] + step1[13]) * rvp9_cospi64[16];
  step2[10] = RVP9_WRAP(rvp9_round_shift(temp1));
  step2[13] = RVP9_WRAP(rvp9_round_shift(temp2));
  temp1 = (-step1[11] + step1[12]) * rvp9_cospi64[16];
  temp2 = (step1[11] + step1[12]) * rvp9_cospi64[16];
  step2[11] = RVP9_WRAP(rvp9_round_shift(temp1));
  step2[12] = RVP9_WRAP(rvp9_round_shift(temp2));
  step2[14] = step1[14];
  step2[15] = step1[15];

/* stage 7 */
  output[0] = (rvp9_tran)RVP9_WRAP(step2[0] + step2[15]);
  output[1] = (rvp9_tran)RVP9_WRAP(step2[1] + step2[14]);
  output[2] = (rvp9_tran)RVP9_WRAP(step2[2] + step2[13]);
  output[3] = (rvp9_tran)RVP9_WRAP(step2[3] + step2[12]);
  output[4] = (rvp9_tran)RVP9_WRAP(step2[4] + step2[11]);
  output[5] = (rvp9_tran)RVP9_WRAP(step2[5] + step2[10]);
  output[6] = (rvp9_tran)RVP9_WRAP(step2[6] + step2[9]);
  output[7] = (rvp9_tran)RVP9_WRAP(step2[7] + step2[8]);
  output[8] = (rvp9_tran)RVP9_WRAP(step2[7] - step2[8]);
  output[9] = (rvp9_tran)RVP9_WRAP(step2[6] - step2[9]);
  output[10] = (rvp9_tran)RVP9_WRAP(step2[5] - step2[10]);
  output[11] = (rvp9_tran)RVP9_WRAP(step2[4] - step2[11]);
  output[12] = (rvp9_tran)RVP9_WRAP(step2[3] - step2[12]);
  output[13] = (rvp9_tran)RVP9_WRAP(step2[2] - step2[13]);
  output[14] = (rvp9_tran)RVP9_WRAP(step2[1] - step2[14]);
  output[15] = (rvp9_tran)RVP9_WRAP(step2[0] - step2[15]);
}

static void rvp9_iadst16(const rvp9_tran *input, rvp9_tran *output) {
  int64_t s0, s1, s2, s3, s4, s5, s6, s7, s8;
  int64_t s9, s10, s11, s12, s13, s14, s15;
  int64_t x0 = input[15];
  int64_t x1 = input[0];
  int64_t x2 = input[13];
  int64_t x3 = input[2];
  int64_t x4 = input[11];
  int64_t x5 = input[4];
  int64_t x6 = input[9];
  int64_t x7 = input[6];
  int64_t x8 = input[7];
  int64_t x9 = input[8];
  int64_t x10 = input[5];
  int64_t x11 = input[10];
  int64_t x12 = input[3];
  int64_t x13 = input[12];
  int64_t x14 = input[1];
  int64_t x15 = input[14];

  if (!(x0 | x1 | x2 | x3 | x4 | x5 | x6 | x7 | x8 | x9 | x10 | x11 | x12 |
        x13 | x14 | x15)) {
    memset(output, 0, 16 * sizeof(*output));
    return;
  }

/* stage 1 */
  s0 = x0 * rvp9_cospi64[1] + x1 * rvp9_cospi64[31];
  s1 = x0 * rvp9_cospi64[31] - x1 * rvp9_cospi64[1];
  s2 = x2 * rvp9_cospi64[5] + x3 * rvp9_cospi64[27];
  s3 = x2 * rvp9_cospi64[27] - x3 * rvp9_cospi64[5];
  s4 = x4 * rvp9_cospi64[9] + x5 * rvp9_cospi64[23];
  s5 = x4 * rvp9_cospi64[23] - x5 * rvp9_cospi64[9];
  s6 = x6 * rvp9_cospi64[13] + x7 * rvp9_cospi64[19];
  s7 = x6 * rvp9_cospi64[19] - x7 * rvp9_cospi64[13];
  s8 = x8 * rvp9_cospi64[17] + x9 * rvp9_cospi64[15];
  s9 = x8 * rvp9_cospi64[15] - x9 * rvp9_cospi64[17];
  s10 = x10 * rvp9_cospi64[21] + x11 * rvp9_cospi64[11];
  s11 = x10 * rvp9_cospi64[11] - x11 * rvp9_cospi64[21];
  s12 = x12 * rvp9_cospi64[25] + x13 * rvp9_cospi64[7];
  s13 = x12 * rvp9_cospi64[7] - x13 * rvp9_cospi64[25];
  s14 = x14 * rvp9_cospi64[29] + x15 * rvp9_cospi64[3];
  s15 = x14 * rvp9_cospi64[3] - x15 * rvp9_cospi64[29];

  x0 = RVP9_WRAP(rvp9_round_shift(s0 + s8));
  x1 = RVP9_WRAP(rvp9_round_shift(s1 + s9));
  x2 = RVP9_WRAP(rvp9_round_shift(s2 + s10));
  x3 = RVP9_WRAP(rvp9_round_shift(s3 + s11));
  x4 = RVP9_WRAP(rvp9_round_shift(s4 + s12));
  x5 = RVP9_WRAP(rvp9_round_shift(s5 + s13));
  x6 = RVP9_WRAP(rvp9_round_shift(s6 + s14));
  x7 = RVP9_WRAP(rvp9_round_shift(s7 + s15));
  x8 = RVP9_WRAP(rvp9_round_shift(s0 - s8));
  x9 = RVP9_WRAP(rvp9_round_shift(s1 - s9));
  x10 = RVP9_WRAP(rvp9_round_shift(s2 - s10));
  x11 = RVP9_WRAP(rvp9_round_shift(s3 - s11));
  x12 = RVP9_WRAP(rvp9_round_shift(s4 - s12));
  x13 = RVP9_WRAP(rvp9_round_shift(s5 - s13));
  x14 = RVP9_WRAP(rvp9_round_shift(s6 - s14));
  x15 = RVP9_WRAP(rvp9_round_shift(s7 - s15));

/* stage 2 */
  s0 = x0;
  s1 = x1;
  s2 = x2;
  s3 = x3;
  s4 = x4;
  s5 = x5;
  s6 = x6;
  s7 = x7;
  s8 = x8 * rvp9_cospi64[4] + x9 * rvp9_cospi64[28];
  s9 = x8 * rvp9_cospi64[28] - x9 * rvp9_cospi64[4];
  s10 = x10 * rvp9_cospi64[20] + x11 * rvp9_cospi64[12];
  s11 = x10 * rvp9_cospi64[12] - x11 * rvp9_cospi64[20];
  s12 = -x12 * rvp9_cospi64[28] + x13 * rvp9_cospi64[4];
  s13 = x12 * rvp9_cospi64[4] + x13 * rvp9_cospi64[28];
  s14 = -x14 * rvp9_cospi64[12] + x15 * rvp9_cospi64[20];
  s15 = x14 * rvp9_cospi64[20] + x15 * rvp9_cospi64[12];

  x0 = RVP9_WRAP(s0 + s4);
  x1 = RVP9_WRAP(s1 + s5);
  x2 = RVP9_WRAP(s2 + s6);
  x3 = RVP9_WRAP(s3 + s7);
  x4 = RVP9_WRAP(s0 - s4);
  x5 = RVP9_WRAP(s1 - s5);
  x6 = RVP9_WRAP(s2 - s6);
  x7 = RVP9_WRAP(s3 - s7);
  x8 = RVP9_WRAP(rvp9_round_shift(s8 + s12));
  x9 = RVP9_WRAP(rvp9_round_shift(s9 + s13));
  x10 = RVP9_WRAP(rvp9_round_shift(s10 + s14));
  x11 = RVP9_WRAP(rvp9_round_shift(s11 + s15));
  x12 = RVP9_WRAP(rvp9_round_shift(s8 - s12));
  x13 = RVP9_WRAP(rvp9_round_shift(s9 - s13));
  x14 = RVP9_WRAP(rvp9_round_shift(s10 - s14));
  x15 = RVP9_WRAP(rvp9_round_shift(s11 - s15));

/* stage 3 */
  s0 = x0;
  s1 = x1;
  s2 = x2;
  s3 = x3;
  s4 = x4 * rvp9_cospi64[8] + x5 * rvp9_cospi64[24];
  s5 = x4 * rvp9_cospi64[24] - x5 * rvp9_cospi64[8];
  s6 = -x6 * rvp9_cospi64[24] + x7 * rvp9_cospi64[8];
  s7 = x6 * rvp9_cospi64[8] + x7 * rvp9_cospi64[24];
  s8 = x8;
  s9 = x9;
  s10 = x10;
  s11 = x11;
  s12 = x12 * rvp9_cospi64[8] + x13 * rvp9_cospi64[24];
  s13 = x12 * rvp9_cospi64[24] - x13 * rvp9_cospi64[8];
  s14 = -x14 * rvp9_cospi64[24] + x15 * rvp9_cospi64[8];
  s15 = x14 * rvp9_cospi64[8] + x15 * rvp9_cospi64[24];

  x0 = RVP9_WRAP(s0 + s2);
  x1 = RVP9_WRAP(s1 + s3);
  x2 = RVP9_WRAP(s0 - s2);
  x3 = RVP9_WRAP(s1 - s3);
  x4 = RVP9_WRAP(rvp9_round_shift(s4 + s6));
  x5 = RVP9_WRAP(rvp9_round_shift(s5 + s7));
  x6 = RVP9_WRAP(rvp9_round_shift(s4 - s6));
  x7 = RVP9_WRAP(rvp9_round_shift(s5 - s7));
  x8 = RVP9_WRAP(s8 + s10);
  x9 = RVP9_WRAP(s9 + s11);
  x10 = RVP9_WRAP(s8 - s10);
  x11 = RVP9_WRAP(s9 - s11);
  x12 = RVP9_WRAP(rvp9_round_shift(s12 + s14));
  x13 = RVP9_WRAP(rvp9_round_shift(s13 + s15));
  x14 = RVP9_WRAP(rvp9_round_shift(s12 - s14));
  x15 = RVP9_WRAP(rvp9_round_shift(s13 - s15));

/* stage 4 */
  s2 = (-rvp9_cospi64[16]) * (x2 + x3);
  s3 = rvp9_cospi64[16] * (x2 - x3);
  s6 = rvp9_cospi64[16] * (x6 + x7);
  s7 = rvp9_cospi64[16] * (-x6 + x7);
  s10 = rvp9_cospi64[16] * (x10 + x11);
  s11 = rvp9_cospi64[16] * (-x10 + x11);
  s14 = (-rvp9_cospi64[16]) * (x14 + x15);
  s15 = rvp9_cospi64[16] * (x14 - x15);

  x2 = RVP9_WRAP(rvp9_round_shift(s2));
  x3 = RVP9_WRAP(rvp9_round_shift(s3));
  x6 = RVP9_WRAP(rvp9_round_shift(s6));
  x7 = RVP9_WRAP(rvp9_round_shift(s7));
  x10 = RVP9_WRAP(rvp9_round_shift(s10));
  x11 = RVP9_WRAP(rvp9_round_shift(s11));
  x14 = RVP9_WRAP(rvp9_round_shift(s14));
  x15 = RVP9_WRAP(rvp9_round_shift(s15));

  output[0] = RVP9_WRAP(x0);
  output[1] = RVP9_WRAP(-x8);
  output[2] = RVP9_WRAP(x12);
  output[3] = RVP9_WRAP(-x4);
  output[4] = RVP9_WRAP(x6);
  output[5] = RVP9_WRAP(x14);
  output[6] = RVP9_WRAP(x10);
  output[7] = RVP9_WRAP(x2);
  output[8] = RVP9_WRAP(x3);
  output[9] = RVP9_WRAP(x11);
  output[10] = RVP9_WRAP(x15);
  output[11] = RVP9_WRAP(x7);
  output[12] = RVP9_WRAP(x5);
  output[13] = RVP9_WRAP(-x13);
  output[14] = RVP9_WRAP(x9);
  output[15] = RVP9_WRAP(-x1);
}

static void rvp9_idct32(const rvp9_tran *input, rvp9_tran *output) {
  RVP9_TRAN_STEP step1[32], step2[32];
  int64_t temp1, temp2;

/* stage 1 */
  step1[0] = input[0];
  step1[1] = input[16];
  step1[2] = input[8];
  step1[3] = input[24];
  step1[4] = input[4];
  step1[5] = input[20];
  step1[6] = input[12];
  step1[7] = input[28];
  step1[8] = input[2];
  step1[9] = input[18];
  step1[10] = input[10];
  step1[11] = input[26];
  step1[12] = input[6];
  step1[13] = input[22];
  step1[14] = input[14];
  step1[15] = input[30];

  temp1 = input[1] * rvp9_cospi64[31] - input[31] * rvp9_cospi64[1];
  temp2 = input[1] * rvp9_cospi64[1] + input[31] * rvp9_cospi64[31];
  step1[16] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[31] = RVP9_WRAP(rvp9_round_shift(temp2));

  temp1 = input[17] * rvp9_cospi64[15] - input[15] * rvp9_cospi64[17];
  temp2 = input[17] * rvp9_cospi64[17] + input[15] * rvp9_cospi64[15];
  step1[17] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[30] = RVP9_WRAP(rvp9_round_shift(temp2));

  temp1 = input[9] * rvp9_cospi64[23] - input[23] * rvp9_cospi64[9];
  temp2 = input[9] * rvp9_cospi64[9] + input[23] * rvp9_cospi64[23];
  step1[18] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[29] = RVP9_WRAP(rvp9_round_shift(temp2));

  temp1 = input[25] * rvp9_cospi64[7] - input[7] * rvp9_cospi64[25];
  temp2 = input[25] * rvp9_cospi64[25] + input[7] * rvp9_cospi64[7];
  step1[19] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[28] = RVP9_WRAP(rvp9_round_shift(temp2));

  temp1 = input[5] * rvp9_cospi64[27] - input[27] * rvp9_cospi64[5];
  temp2 = input[5] * rvp9_cospi64[5] + input[27] * rvp9_cospi64[27];
  step1[20] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[27] = RVP9_WRAP(rvp9_round_shift(temp2));

  temp1 = input[21] * rvp9_cospi64[11] - input[11] * rvp9_cospi64[21];
  temp2 = input[21] * rvp9_cospi64[21] + input[11] * rvp9_cospi64[11];
  step1[21] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[26] = RVP9_WRAP(rvp9_round_shift(temp2));

  temp1 = input[13] * rvp9_cospi64[19] - input[19] * rvp9_cospi64[13];
  temp2 = input[13] * rvp9_cospi64[13] + input[19] * rvp9_cospi64[19];
  step1[22] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[25] = RVP9_WRAP(rvp9_round_shift(temp2));

  temp1 = input[29] * rvp9_cospi64[3] - input[3] * rvp9_cospi64[29];
  temp2 = input[29] * rvp9_cospi64[29] + input[3] * rvp9_cospi64[3];
  step1[23] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[24] = RVP9_WRAP(rvp9_round_shift(temp2));

/* stage 2 */
  step2[0] = step1[0];
  step2[1] = step1[1];
  step2[2] = step1[2];
  step2[3] = step1[3];
  step2[4] = step1[4];
  step2[5] = step1[5];
  step2[6] = step1[6];
  step2[7] = step1[7];

  temp1 = step1[8] * rvp9_cospi64[30] - step1[15] * rvp9_cospi64[2];
  temp2 = step1[8] * rvp9_cospi64[2] + step1[15] * rvp9_cospi64[30];
  step2[8] = RVP9_WRAP(rvp9_round_shift(temp1));
  step2[15] = RVP9_WRAP(rvp9_round_shift(temp2));

  temp1 = step1[9] * rvp9_cospi64[14] - step1[14] * rvp9_cospi64[18];
  temp2 = step1[9] * rvp9_cospi64[18] + step1[14] * rvp9_cospi64[14];
  step2[9] = RVP9_WRAP(rvp9_round_shift(temp1));
  step2[14] = RVP9_WRAP(rvp9_round_shift(temp2));

  temp1 = step1[10] * rvp9_cospi64[22] - step1[13] * rvp9_cospi64[10];
  temp2 = step1[10] * rvp9_cospi64[10] + step1[13] * rvp9_cospi64[22];
  step2[10] = RVP9_WRAP(rvp9_round_shift(temp1));
  step2[13] = RVP9_WRAP(rvp9_round_shift(temp2));

  temp1 = step1[11] * rvp9_cospi64[6] - step1[12] * rvp9_cospi64[26];
  temp2 = step1[11] * rvp9_cospi64[26] + step1[12] * rvp9_cospi64[6];
  step2[11] = RVP9_WRAP(rvp9_round_shift(temp1));
  step2[12] = RVP9_WRAP(rvp9_round_shift(temp2));

  step2[16] = RVP9_WRAP(step1[16] + step1[17]);
  step2[17] = RVP9_WRAP(step1[16] - step1[17]);
  step2[18] = RVP9_WRAP(-step1[18] + step1[19]);
  step2[19] = RVP9_WRAP(step1[18] + step1[19]);
  step2[20] = RVP9_WRAP(step1[20] + step1[21]);
  step2[21] = RVP9_WRAP(step1[20] - step1[21]);
  step2[22] = RVP9_WRAP(-step1[22] + step1[23]);
  step2[23] = RVP9_WRAP(step1[22] + step1[23]);
  step2[24] = RVP9_WRAP(step1[24] + step1[25]);
  step2[25] = RVP9_WRAP(step1[24] - step1[25]);
  step2[26] = RVP9_WRAP(-step1[26] + step1[27]);
  step2[27] = RVP9_WRAP(step1[26] + step1[27]);
  step2[28] = RVP9_WRAP(step1[28] + step1[29]);
  step2[29] = RVP9_WRAP(step1[28] - step1[29]);
  step2[30] = RVP9_WRAP(-step1[30] + step1[31]);
  step2[31] = RVP9_WRAP(step1[30] + step1[31]);

/* stage 3 */
  step1[0] = step2[0];
  step1[1] = step2[1];
  step1[2] = step2[2];
  step1[3] = step2[3];

  temp1 = step2[4] * rvp9_cospi64[28] - step2[7] * rvp9_cospi64[4];
  temp2 = step2[4] * rvp9_cospi64[4] + step2[7] * rvp9_cospi64[28];
  step1[4] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[7] = RVP9_WRAP(rvp9_round_shift(temp2));
  temp1 = step2[5] * rvp9_cospi64[12] - step2[6] * rvp9_cospi64[20];
  temp2 = step2[5] * rvp9_cospi64[20] + step2[6] * rvp9_cospi64[12];
  step1[5] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[6] = RVP9_WRAP(rvp9_round_shift(temp2));

  step1[8] = RVP9_WRAP(step2[8] + step2[9]);
  step1[9] = RVP9_WRAP(step2[8] - step2[9]);
  step1[10] = RVP9_WRAP(-step2[10] + step2[11]);
  step1[11] = RVP9_WRAP(step2[10] + step2[11]);
  step1[12] = RVP9_WRAP(step2[12] + step2[13]);
  step1[13] = RVP9_WRAP(step2[12] - step2[13]);
  step1[14] = RVP9_WRAP(-step2[14] + step2[15]);
  step1[15] = RVP9_WRAP(step2[14] + step2[15]);

  step1[16] = step2[16];
  step1[31] = step2[31];
  temp1 = -step2[17] * rvp9_cospi64[4] + step2[30] * rvp9_cospi64[28];
  temp2 = step2[17] * rvp9_cospi64[28] + step2[30] * rvp9_cospi64[4];
  step1[17] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[30] = RVP9_WRAP(rvp9_round_shift(temp2));
  temp1 = -step2[18] * rvp9_cospi64[28] - step2[29] * rvp9_cospi64[4];
  temp2 = -step2[18] * rvp9_cospi64[4] + step2[29] * rvp9_cospi64[28];
  step1[18] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[29] = RVP9_WRAP(rvp9_round_shift(temp2));
  step1[19] = step2[19];
  step1[20] = step2[20];
  temp1 = -step2[21] * rvp9_cospi64[20] + step2[26] * rvp9_cospi64[12];
  temp2 = step2[21] * rvp9_cospi64[12] + step2[26] * rvp9_cospi64[20];
  step1[21] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[26] = RVP9_WRAP(rvp9_round_shift(temp2));
  temp1 = -step2[22] * rvp9_cospi64[12] - step2[25] * rvp9_cospi64[20];
  temp2 = -step2[22] * rvp9_cospi64[20] + step2[25] * rvp9_cospi64[12];
  step1[22] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[25] = RVP9_WRAP(rvp9_round_shift(temp2));
  step1[23] = step2[23];
  step1[24] = step2[24];
  step1[27] = step2[27];
  step1[28] = step2[28];

/* stage 4 */
  temp1 = (step1[0] + step1[1]) * rvp9_cospi64[16];
  temp2 = (step1[0] - step1[1]) * rvp9_cospi64[16];
  step2[0] = RVP9_WRAP(rvp9_round_shift(temp1));
  step2[1] = RVP9_WRAP(rvp9_round_shift(temp2));
  temp1 = step1[2] * rvp9_cospi64[24] - step1[3] * rvp9_cospi64[8];
  temp2 = step1[2] * rvp9_cospi64[8] + step1[3] * rvp9_cospi64[24];
  step2[2] = RVP9_WRAP(rvp9_round_shift(temp1));
  step2[3] = RVP9_WRAP(rvp9_round_shift(temp2));
  step2[4] = RVP9_WRAP(step1[4] + step1[5]);
  step2[5] = RVP9_WRAP(step1[4] - step1[5]);
  step2[6] = RVP9_WRAP(-step1[6] + step1[7]);
  step2[7] = RVP9_WRAP(step1[6] + step1[7]);

  step2[8] = step1[8];
  step2[15] = step1[15];
  temp1 = -step1[9] * rvp9_cospi64[8] + step1[14] * rvp9_cospi64[24];
  temp2 = step1[9] * rvp9_cospi64[24] + step1[14] * rvp9_cospi64[8];
  step2[9] = RVP9_WRAP(rvp9_round_shift(temp1));
  step2[14] = RVP9_WRAP(rvp9_round_shift(temp2));
  temp1 = -step1[10] * rvp9_cospi64[24] - step1[13] * rvp9_cospi64[8];
  temp2 = -step1[10] * rvp9_cospi64[8] + step1[13] * rvp9_cospi64[24];
  step2[10] = RVP9_WRAP(rvp9_round_shift(temp1));
  step2[13] = RVP9_WRAP(rvp9_round_shift(temp2));
  step2[11] = step1[11];
  step2[12] = step1[12];

  step2[16] = RVP9_WRAP(step1[16] + step1[19]);
  step2[17] = RVP9_WRAP(step1[17] + step1[18]);
  step2[18] = RVP9_WRAP(step1[17] - step1[18]);
  step2[19] = RVP9_WRAP(step1[16] - step1[19]);
  step2[20] = RVP9_WRAP(-step1[20] + step1[23]);
  step2[21] = RVP9_WRAP(-step1[21] + step1[22]);
  step2[22] = RVP9_WRAP(step1[21] + step1[22]);
  step2[23] = RVP9_WRAP(step1[20] + step1[23]);

  step2[24] = RVP9_WRAP(step1[24] + step1[27]);
  step2[25] = RVP9_WRAP(step1[25] + step1[26]);
  step2[26] = RVP9_WRAP(step1[25] - step1[26]);
  step2[27] = RVP9_WRAP(step1[24] - step1[27]);
  step2[28] = RVP9_WRAP(-step1[28] + step1[31]);
  step2[29] = RVP9_WRAP(-step1[29] + step1[30]);
  step2[30] = RVP9_WRAP(step1[29] + step1[30]);
  step2[31] = RVP9_WRAP(step1[28] + step1[31]);

/* stage 5 */
  step1[0] = RVP9_WRAP(step2[0] + step2[3]);
  step1[1] = RVP9_WRAP(step2[1] + step2[2]);
  step1[2] = RVP9_WRAP(step2[1] - step2[2]);
  step1[3] = RVP9_WRAP(step2[0] - step2[3]);
  step1[4] = step2[4];
  temp1 = (step2[6] - step2[5]) * rvp9_cospi64[16];
  temp2 = (step2[5] + step2[6]) * rvp9_cospi64[16];
  step1[5] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[6] = RVP9_WRAP(rvp9_round_shift(temp2));
  step1[7] = step2[7];

  step1[8] = RVP9_WRAP(step2[8] + step2[11]);
  step1[9] = RVP9_WRAP(step2[9] + step2[10]);
  step1[10] = RVP9_WRAP(step2[9] - step2[10]);
  step1[11] = RVP9_WRAP(step2[8] - step2[11]);
  step1[12] = RVP9_WRAP(-step2[12] + step2[15]);
  step1[13] = RVP9_WRAP(-step2[13] + step2[14]);
  step1[14] = RVP9_WRAP(step2[13] + step2[14]);
  step1[15] = RVP9_WRAP(step2[12] + step2[15]);

  step1[16] = step2[16];
  step1[17] = step2[17];
  temp1 = -step2[18] * rvp9_cospi64[8] + step2[29] * rvp9_cospi64[24];
  temp2 = step2[18] * rvp9_cospi64[24] + step2[29] * rvp9_cospi64[8];
  step1[18] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[29] = RVP9_WRAP(rvp9_round_shift(temp2));
  temp1 = -step2[19] * rvp9_cospi64[8] + step2[28] * rvp9_cospi64[24];
  temp2 = step2[19] * rvp9_cospi64[24] + step2[28] * rvp9_cospi64[8];
  step1[19] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[28] = RVP9_WRAP(rvp9_round_shift(temp2));
  temp1 = -step2[20] * rvp9_cospi64[24] - step2[27] * rvp9_cospi64[8];
  temp2 = -step2[20] * rvp9_cospi64[8] + step2[27] * rvp9_cospi64[24];
  step1[20] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[27] = RVP9_WRAP(rvp9_round_shift(temp2));
  temp1 = -step2[21] * rvp9_cospi64[24] - step2[26] * rvp9_cospi64[8];
  temp2 = -step2[21] * rvp9_cospi64[8] + step2[26] * rvp9_cospi64[24];
  step1[21] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[26] = RVP9_WRAP(rvp9_round_shift(temp2));
  step1[22] = step2[22];
  step1[23] = step2[23];
  step1[24] = step2[24];
  step1[25] = step2[25];
  step1[30] = step2[30];
  step1[31] = step2[31];

/* stage 6 */
  step2[0] = RVP9_WRAP(step1[0] + step1[7]);
  step2[1] = RVP9_WRAP(step1[1] + step1[6]);
  step2[2] = RVP9_WRAP(step1[2] + step1[5]);
  step2[3] = RVP9_WRAP(step1[3] + step1[4]);
  step2[4] = RVP9_WRAP(step1[3] - step1[4]);
  step2[5] = RVP9_WRAP(step1[2] - step1[5]);
  step2[6] = RVP9_WRAP(step1[1] - step1[6]);
  step2[7] = RVP9_WRAP(step1[0] - step1[7]);
  step2[8] = step1[8];
  step2[9] = step1[9];
  temp1 = (-step1[10] + step1[13]) * rvp9_cospi64[16];
  temp2 = (step1[10] + step1[13]) * rvp9_cospi64[16];
  step2[10] = RVP9_WRAP(rvp9_round_shift(temp1));
  step2[13] = RVP9_WRAP(rvp9_round_shift(temp2));
  temp1 = (-step1[11] + step1[12]) * rvp9_cospi64[16];
  temp2 = (step1[11] + step1[12]) * rvp9_cospi64[16];
  step2[11] = RVP9_WRAP(rvp9_round_shift(temp1));
  step2[12] = RVP9_WRAP(rvp9_round_shift(temp2));
  step2[14] = step1[14];
  step2[15] = step1[15];

  step2[16] = RVP9_WRAP(step1[16] + step1[23]);
  step2[17] = RVP9_WRAP(step1[17] + step1[22]);
  step2[18] = RVP9_WRAP(step1[18] + step1[21]);
  step2[19] = RVP9_WRAP(step1[19] + step1[20]);
  step2[20] = RVP9_WRAP(step1[19] - step1[20]);
  step2[21] = RVP9_WRAP(step1[18] - step1[21]);
  step2[22] = RVP9_WRAP(step1[17] - step1[22]);
  step2[23] = RVP9_WRAP(step1[16] - step1[23]);

  step2[24] = RVP9_WRAP(-step1[24] + step1[31]);
  step2[25] = RVP9_WRAP(-step1[25] + step1[30]);
  step2[26] = RVP9_WRAP(-step1[26] + step1[29]);
  step2[27] = RVP9_WRAP(-step1[27] + step1[28]);
  step2[28] = RVP9_WRAP(step1[27] + step1[28]);
  step2[29] = RVP9_WRAP(step1[26] + step1[29]);
  step2[30] = RVP9_WRAP(step1[25] + step1[30]);
  step2[31] = RVP9_WRAP(step1[24] + step1[31]);

/* stage 7 */
  step1[0] = RVP9_WRAP(step2[0] + step2[15]);
  step1[1] = RVP9_WRAP(step2[1] + step2[14]);
  step1[2] = RVP9_WRAP(step2[2] + step2[13]);
  step1[3] = RVP9_WRAP(step2[3] + step2[12]);
  step1[4] = RVP9_WRAP(step2[4] + step2[11]);
  step1[5] = RVP9_WRAP(step2[5] + step2[10]);
  step1[6] = RVP9_WRAP(step2[6] + step2[9]);
  step1[7] = RVP9_WRAP(step2[7] + step2[8]);
  step1[8] = RVP9_WRAP(step2[7] - step2[8]);
  step1[9] = RVP9_WRAP(step2[6] - step2[9]);
  step1[10] = RVP9_WRAP(step2[5] - step2[10]);
  step1[11] = RVP9_WRAP(step2[4] - step2[11]);
  step1[12] = RVP9_WRAP(step2[3] - step2[12]);
  step1[13] = RVP9_WRAP(step2[2] - step2[13]);
  step1[14] = RVP9_WRAP(step2[1] - step2[14]);
  step1[15] = RVP9_WRAP(step2[0] - step2[15]);

  step1[16] = step2[16];
  step1[17] = step2[17];
  step1[18] = step2[18];
  step1[19] = step2[19];
  temp1 = (-step2[20] + step2[27]) * rvp9_cospi64[16];
  temp2 = (step2[20] + step2[27]) * rvp9_cospi64[16];
  step1[20] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[27] = RVP9_WRAP(rvp9_round_shift(temp2));
  temp1 = (-step2[21] + step2[26]) * rvp9_cospi64[16];
  temp2 = (step2[21] + step2[26]) * rvp9_cospi64[16];
  step1[21] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[26] = RVP9_WRAP(rvp9_round_shift(temp2));
  temp1 = (-step2[22] + step2[25]) * rvp9_cospi64[16];
  temp2 = (step2[22] + step2[25]) * rvp9_cospi64[16];
  step1[22] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[25] = RVP9_WRAP(rvp9_round_shift(temp2));
  temp1 = (-step2[23] + step2[24]) * rvp9_cospi64[16];
  temp2 = (step2[23] + step2[24]) * rvp9_cospi64[16];
  step1[23] = RVP9_WRAP(rvp9_round_shift(temp1));
  step1[24] = RVP9_WRAP(rvp9_round_shift(temp2));
  step1[28] = step2[28];
  step1[29] = step2[29];
  step1[30] = step2[30];
  step1[31] = step2[31];

/* final stage */
  output[0] = RVP9_WRAP(step1[0] + step1[31]);
  output[1] = RVP9_WRAP(step1[1] + step1[30]);
  output[2] = RVP9_WRAP(step1[2] + step1[29]);
  output[3] = RVP9_WRAP(step1[3] + step1[28]);
  output[4] = RVP9_WRAP(step1[4] + step1[27]);
  output[5] = RVP9_WRAP(step1[5] + step1[26]);
  output[6] = RVP9_WRAP(step1[6] + step1[25]);
  output[7] = RVP9_WRAP(step1[7] + step1[24]);
  output[8] = RVP9_WRAP(step1[8] + step1[23]);
  output[9] = RVP9_WRAP(step1[9] + step1[22]);
  output[10] = RVP9_WRAP(step1[10] + step1[21]);
  output[11] = RVP9_WRAP(step1[11] + step1[20]);
  output[12] = RVP9_WRAP(step1[12] + step1[19]);
  output[13] = RVP9_WRAP(step1[13] + step1[18]);
  output[14] = RVP9_WRAP(step1[14] + step1[17]);
  output[15] = RVP9_WRAP(step1[15] + step1[16]);
  output[16] = RVP9_WRAP(step1[15] - step1[16]);
  output[17] = RVP9_WRAP(step1[14] - step1[17]);
  output[18] = RVP9_WRAP(step1[13] - step1[18]);
  output[19] = RVP9_WRAP(step1[12] - step1[19]);
  output[20] = RVP9_WRAP(step1[11] - step1[20]);
  output[21] = RVP9_WRAP(step1[10] - step1[21]);
  output[22] = RVP9_WRAP(step1[9] - step1[22]);
  output[23] = RVP9_WRAP(step1[8] - step1[23]);
  output[24] = RVP9_WRAP(step1[7] - step1[24]);
  output[25] = RVP9_WRAP(step1[6] - step1[25]);
  output[26] = RVP9_WRAP(step1[5] - step1[26]);
  output[27] = RVP9_WRAP(step1[4] - step1[27]);
  output[28] = RVP9_WRAP(step1[3] - step1[28]);
  output[29] = RVP9_WRAP(step1[2] - step1[29]);
  output[30] = RVP9_WRAP(step1[1] - step1[30]);
  output[31] = RVP9_WRAP(step1[0] - step1[31]);
}


/* ==================================================================== */
/* Intra predictors, ported from vpx_dsp/intrapred.c (generic C).       */
/* ==================================================================== */
#define RVP9_AVG3(a, b, c) (((a) + 2 * (b) + (c) + 2) >> 2)
#define RVP9_AVG2(a, b)    (((a) + (b) + 1) >> 1)

static void rvp9_d207_predictor(RVP9_PIXEL *dst, int stride, int bs,
                                  const RVP9_PIXEL *above, const RVP9_PIXEL *left) {
  int r, c;
  (void)above;
/* first column */
  for (r = 0; r < bs - 1; ++r) dst[r * stride] = RVP9_AVG2(left[r], left[r + 1]);
  dst[(bs - 1) * stride] = left[bs - 1];
  dst++;

/* second column */
  for (r = 0; r < bs - 2; ++r)
    dst[r * stride] = RVP9_AVG3(left[r], left[r + 1], left[r + 2]);
  dst[(bs - 2) * stride] = RVP9_AVG3(left[bs - 2], left[bs - 1], left[bs - 1]);
  dst[(bs - 1) * stride] = left[bs - 1];
  dst++;

/* rest of last row */
  for (c = 0; c < bs - 2; ++c) dst[(bs - 1) * stride + c] = left[bs - 1];

  for (r = bs - 2; r >= 0; --r)
    for (c = 0; c < bs - 2; ++c)
      dst[r * stride + c] = dst[(r + 1) * stride + c - 2];
}

static void rvp9_d63_predictor(RVP9_PIXEL *dst, int stride, int bs,
                                 const RVP9_PIXEL *above, const RVP9_PIXEL *left) {
  int r, c;
  int size;
  (void)left;
  for (c = 0; c < bs; ++c) {
    dst[c] = RVP9_AVG2(above[c], above[c + 1]);
    dst[stride + c] = RVP9_AVG3(above[c], above[c + 1], above[c + 2]);
  }
  for (r = 2, size = bs - 2; r < bs; r += 2, --size) {
    RVP9_PXCOPY(dst + (r + 0) * stride, dst + (r >> 1), size);
    RVP9_PXFILL(dst + (r + 0) * stride + size, above[bs - 1], bs - size);
    RVP9_PXCOPY(dst + (r + 1) * stride, dst + stride + (r >> 1), size);
    RVP9_PXFILL(dst + (r + 1) * stride + size, above[bs - 1], bs - size);
  }
}

static void rvp9_d45_predictor(RVP9_PIXEL *dst, int stride, int bs,
                                 const RVP9_PIXEL *above, const RVP9_PIXEL *left) {
  const RVP9_PIXEL above_right = above[bs - 1];
  const RVP9_PIXEL *const dst_row0 = dst;
  int x, size;
  (void)left;

  for (x = 0; x < bs - 1; ++x) {
    dst[x] = RVP9_AVG3(above[x], above[x + 1], above[x + 2]);
  }
  dst[bs - 1] = above_right;
  dst += stride;
  for (x = 1, size = bs - 2; x < bs; ++x, --size) {
    RVP9_PXCOPY(dst, dst_row0 + x, size);
    RVP9_PXFILL(dst + size, above_right, x + 1);
    dst += stride;
  }
}

static void rvp9_d117_predictor(RVP9_PIXEL *dst, int stride, int bs,
                                  const RVP9_PIXEL *above, const RVP9_PIXEL *left) {
  int r, c;

/* first row */
  for (c = 0; c < bs; c++) dst[c] = RVP9_AVG2(above[c - 1], above[c]);
  dst += stride;

/* second row */
  dst[0] = RVP9_AVG3(left[0], above[-1], above[0]);
  for (c = 1; c < bs; c++) dst[c] = RVP9_AVG3(above[c - 2], above[c - 1], above[c]);
  dst += stride;

/* the rest of first col */
  dst[0] = RVP9_AVG3(above[-1], left[0], left[1]);
  for (r = 3; r < bs; ++r)
    dst[(r - 2) * stride] = RVP9_AVG3(left[r - 3], left[r - 2], left[r - 1]);

/* the rest of the block */
  for (r = 2; r < bs; ++r) {
    for (c = 1; c < bs; c++) dst[c] = dst[-2 * stride + c - 1];
    dst += stride;
  }
}

static void rvp9_d135_predictor(RVP9_PIXEL *dst, int stride, int bs,
                                  const RVP9_PIXEL *above, const RVP9_PIXEL *left) {
  int i;
#if defined(__GNUC__) && __GNUC__ == 4 && __GNUC_MINOR__ > 7
/* silence a spurious -Warray-bounds warning, possibly related to: */
/* https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56273 */
  RVP9_PIXEL border[69];
#else
  RVP9_PIXEL border[32 + 32 - 1];  /* outer border from bottom-left to top-right */
#endif

/* dst(bs, bs - 2)[0], i.e., border starting at bottom-left */
  for (i = 0; i < bs - 2; ++i) {
    border[i] = RVP9_AVG3(left[bs - 3 - i], left[bs - 2 - i], left[bs - 1 - i]);
  }
  border[bs - 2] = RVP9_AVG3(above[-1], left[0], left[1]);
  border[bs - 1] = RVP9_AVG3(left[0], above[-1], above[0]);
  border[bs - 0] = RVP9_AVG3(above[-1], above[0], above[1]);
/* dst[0][2, size), i.e., remaining top border ascending */
  for (i = 0; i < bs - 2; ++i) {
    border[bs + 1 + i] = RVP9_AVG3(above[i], above[i + 1], above[i + 2]);
  }

  for (i = 0; i < bs; ++i) {
    RVP9_PXCOPY(dst + i * stride, border + bs - 1 - i, bs);
  }
}

static void rvp9_d153_predictor(RVP9_PIXEL *dst, int stride, int bs,
                                  const RVP9_PIXEL *above, const RVP9_PIXEL *left) {
  int r, c;
  dst[0] = RVP9_AVG2(above[-1], left[0]);
  for (r = 1; r < bs; r++) dst[r * stride] = RVP9_AVG2(left[r - 1], left[r]);
  dst++;

  dst[0] = RVP9_AVG3(left[0], above[-1], above[0]);
  dst[stride] = RVP9_AVG3(above[-1], left[0], left[1]);
  for (r = 2; r < bs; r++)
    dst[r * stride] = RVP9_AVG3(left[r - 2], left[r - 1], left[r]);
  dst++;

  for (c = 0; c < bs - 2; c++)
    dst[c] = RVP9_AVG3(above[c - 1], above[c], above[c + 1]);
  dst += stride;

  for (r = 1; r < bs; ++r) {
    for (c = 0; c < bs - 2; c++) dst[c] = dst[-stride + c - 2];
    dst += stride;
  }
}

static void rvp9_v_predictor(RVP9_PIXEL *dst, int stride, int bs,
                               const RVP9_PIXEL *above, const RVP9_PIXEL *left) {
  int r;
  (void)left;

  for (r = 0; r < bs; r++) {
    RVP9_PXCOPY(dst, above, bs);
    dst += stride;
  }
}

static void rvp9_h_predictor(RVP9_PIXEL *dst, int stride, int bs,
                               const RVP9_PIXEL *above, const RVP9_PIXEL *left) {
  int r;
  (void)above;

  for (r = 0; r < bs; r++) {
    RVP9_PXFILL(dst, left[r], bs);
    dst += stride;
  }
}

static void rvp9_tm_predictor(RVP9_PIXEL *dst, int stride, int bs,
                                const RVP9_PIXEL *above, const RVP9_PIXEL *left) {
  int r, c;
  int ytop_left = above[-1];

  for (r = 0; r < bs; r++) {
    for (c = 0; c < bs; c++)
      dst[c] = rvp9_clip8(left[r] + above[c] - ytop_left);
    dst += stride;
  }
}

static void rvp9_dc_128_predictor(RVP9_PIXEL *dst, int stride, int bs,
                                    const RVP9_PIXEL *above, const RVP9_PIXEL *left) {
  int r;
  (void)above;
  (void)left;

  for (r = 0; r < bs; r++) {
    RVP9_PXFILL(dst, RVP9_PXMID, bs);
    dst += stride;
  }
}

static void rvp9_dc_left_predictor(RVP9_PIXEL *dst, int stride, int bs,
                                     const RVP9_PIXEL *above,
                                     const RVP9_PIXEL *left) {
  int i, r, expected_dc, sum = 0;
  (void)above;

  for (i = 0; i < bs; i++) sum += left[i];
  expected_dc = (sum + (bs >> 1)) / bs;

  for (r = 0; r < bs; r++) {
    RVP9_PXFILL(dst, expected_dc, bs);
    dst += stride;
  }
}

static void rvp9_dc_top_predictor(RVP9_PIXEL *dst, int stride, int bs,
                                    const RVP9_PIXEL *above, const RVP9_PIXEL *left) {
  int i, r, expected_dc, sum = 0;
  (void)left;

  for (i = 0; i < bs; i++) sum += above[i];
  expected_dc = (sum + (bs >> 1)) / bs;

  for (r = 0; r < bs; r++) {
    RVP9_PXFILL(dst, expected_dc, bs);
    dst += stride;
  }
}

static void rvp9_dc_predictor(RVP9_PIXEL *dst, int stride, int bs,
                                const RVP9_PIXEL *above, const RVP9_PIXEL *left) {
  int i, r, expected_dc, sum = 0;
  const int count = 2 * bs;

  for (i = 0; i < bs; i++) {
    sum += above[i];
    sum += left[i];
  }

  expected_dc = (sum + (count >> 1)) / count;

  for (r = 0; r < bs; r++) {
    RVP9_PXFILL(dst, expected_dc, bs);
    dst += stride;
  }
}


/* Specialized 4x4 directional predictors (vpx_dsp intrapred.c): unlike the
 * generic bs-parameterized versions, these read the full above-right
 * extension (above[4..7]). */
#define DST(x, y) dst[(x) + (y)*stride]

static void rvp9_d207_predictor_4x4(RVP9_PIXEL *dst, int stride,
                              const RVP9_PIXEL *above, const RVP9_PIXEL *left) {
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int L = left[3];
  (void)above;
  DST(0, 0) = RVP9_AVG2(I, J);
  DST(2, 0) = DST(0, 1) = RVP9_AVG2(J, K);
  DST(2, 1) = DST(0, 2) = RVP9_AVG2(K, L);
  DST(1, 0) = RVP9_AVG3(I, J, K);
  DST(3, 0) = DST(1, 1) = RVP9_AVG3(J, K, L);
  DST(3, 1) = DST(1, 2) = RVP9_AVG3(K, L, L);
  DST(3, 2) = DST(2, 2) = DST(0, 3) = DST(1, 3) = DST(2, 3) = DST(3, 3) = L;
}

static void rvp9_d63_predictor_4x4(RVP9_PIXEL *dst, int stride,
                             const RVP9_PIXEL *above, const RVP9_PIXEL *left) {
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];
  const int D = above[3];
  const int E = above[4];
  const int F = above[5];
  const int G = above[6];
  (void)left;
  DST(0, 0) = RVP9_AVG2(A, B);
  DST(1, 0) = DST(0, 2) = RVP9_AVG2(B, C);
  DST(2, 0) = DST(1, 2) = RVP9_AVG2(C, D);
  DST(3, 0) = DST(2, 2) = RVP9_AVG2(D, E);
  DST(3, 2) = RVP9_AVG2(E, F);  /* differs from vp8 */

  DST(0, 1) = RVP9_AVG3(A, B, C);
  DST(1, 1) = DST(0, 3) = RVP9_AVG3(B, C, D);
  DST(2, 1) = DST(1, 3) = RVP9_AVG3(C, D, E);
  DST(3, 1) = DST(2, 3) = RVP9_AVG3(D, E, F);
  DST(3, 3) = RVP9_AVG3(E, F, G);  /* differs from vp8 */
}

static void rvp9_d45_predictor_4x4(RVP9_PIXEL *dst, int stride,
                             const RVP9_PIXEL *above, const RVP9_PIXEL *left) {
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];
  const int D = above[3];
  const int E = above[4];
  const int F = above[5];
  const int G = above[6];
  const int H = above[7];
  (void)stride;
  (void)left;
  DST(0, 0) = RVP9_AVG3(A, B, C);
  DST(1, 0) = DST(0, 1) = RVP9_AVG3(B, C, D);
  DST(2, 0) = DST(1, 1) = DST(0, 2) = RVP9_AVG3(C, D, E);
  DST(3, 0) = DST(2, 1) = DST(1, 2) = DST(0, 3) = RVP9_AVG3(D, E, F);
  DST(3, 1) = DST(2, 2) = DST(1, 3) = RVP9_AVG3(E, F, G);
  DST(3, 2) = DST(2, 3) = RVP9_AVG3(F, G, H);
  DST(3, 3) = H;  /* differs from vp8 */
}

static void rvp9_d117_predictor_4x4(RVP9_PIXEL *dst, int stride,
                              const RVP9_PIXEL *above, const RVP9_PIXEL *left) {
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int X = above[-1];
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];
  const int D = above[3];
  DST(0, 0) = DST(1, 2) = RVP9_AVG2(X, A);
  DST(1, 0) = DST(2, 2) = RVP9_AVG2(A, B);
  DST(2, 0) = DST(3, 2) = RVP9_AVG2(B, C);
  DST(3, 0) = RVP9_AVG2(C, D);

  DST(0, 3) = RVP9_AVG3(K, J, I);
  DST(0, 2) = RVP9_AVG3(J, I, X);
  DST(0, 1) = DST(1, 3) = RVP9_AVG3(I, X, A);
  DST(1, 1) = DST(2, 3) = RVP9_AVG3(X, A, B);
  DST(2, 1) = DST(3, 3) = RVP9_AVG3(A, B, C);
  DST(3, 1) = RVP9_AVG3(B, C, D);
}

static void rvp9_d135_predictor_4x4(RVP9_PIXEL *dst, int stride,
                              const RVP9_PIXEL *above, const RVP9_PIXEL *left) {
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int L = left[3];
  const int X = above[-1];
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];
  const int D = above[3];
  (void)stride;
  DST(0, 3) = RVP9_AVG3(J, K, L);
  DST(1, 3) = DST(0, 2) = RVP9_AVG3(I, J, K);
  DST(2, 3) = DST(1, 2) = DST(0, 1) = RVP9_AVG3(X, I, J);
  DST(3, 3) = DST(2, 2) = DST(1, 1) = DST(0, 0) = RVP9_AVG3(A, X, I);
  DST(3, 2) = DST(2, 1) = DST(1, 0) = RVP9_AVG3(B, A, X);
  DST(3, 1) = DST(2, 0) = RVP9_AVG3(C, B, A);
  DST(3, 0) = RVP9_AVG3(D, C, B);
}

static void rvp9_d153_predictor_4x4(RVP9_PIXEL *dst, int stride,
                              const RVP9_PIXEL *above, const RVP9_PIXEL *left) {
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int L = left[3];
  const int X = above[-1];
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];

  DST(0, 0) = DST(2, 1) = RVP9_AVG2(I, X);
  DST(0, 1) = DST(2, 2) = RVP9_AVG2(J, I);
  DST(0, 2) = DST(2, 3) = RVP9_AVG2(K, J);
  DST(0, 3) = RVP9_AVG2(L, K);

  DST(3, 0) = RVP9_AVG3(A, B, C);
  DST(2, 0) = RVP9_AVG3(X, A, B);
  DST(1, 0) = DST(3, 1) = RVP9_AVG3(I, X, A);
  DST(1, 1) = DST(3, 2) = RVP9_AVG3(J, I, X);
  DST(1, 2) = DST(3, 3) = RVP9_AVG3(K, J, I);
  DST(1, 3) = RVP9_AVG3(L, K, J);
}
#undef DST

/* ==================================================================== */
/* IHT wrappers: row pass, column pass, shift + add to destination.     */
/* Ported from vpx_dsp inv_txfm.c vpx_iht/idct *_add functions.         */
/* tx_type: 0=DCT_DCT 1=ADST_DCT 2=DCT_ADST 3=ADST_ADST                 */
/* ==================================================================== */
#if !RVP9_RECON_HBD  /* rvp9_tran is shared; typedef once */
typedef void (*rvp9_tx1d)(const rvp9_tran *, rvp9_tran *);
#endif

static INLINE int rvp9_rp2(int v, int n)   /* ROUND_POWER_OF_TWO */
{
   return (v + (1 << (n - 1))) >> n;
}

static void rvp9_iht_add(const rvp9_tran *input, RVP9_PIXEL *dst, int stride,
      int tx_size, int tx_type, int eob)
{
   static const rvp9_tx1d row1d[4][4] = {
      { rvp9_idct4, rvp9_idct4, rvp9_iadst4, rvp9_iadst4 },
      { rvp9_idct8, rvp9_idct8, rvp9_iadst8, rvp9_iadst8 },
      { rvp9_idct16, rvp9_idct16, rvp9_iadst16, rvp9_iadst16 },
      { rvp9_idct32, rvp9_idct32, rvp9_idct32, rvp9_idct32 }
   };
   static const rvp9_tx1d col1d[4][4] = {
      { rvp9_idct4, rvp9_iadst4, rvp9_idct4, rvp9_iadst4 },
      { rvp9_idct8, rvp9_iadst8, rvp9_idct8, rvp9_iadst8 },
      { rvp9_idct16, rvp9_iadst16, rvp9_idct16, rvp9_iadst16 },
      { rvp9_idct32, rvp9_idct32, rvp9_idct32, rvp9_idct32 }
   };
   rvp9_tran out[32 * 32], tmpi[32], tmpo[32];
   int n = 4 << tx_size;
   int shift = (tx_size == 0) ? 4 : (tx_size == 1) ? 5 : 6;
   int i, j;
   (void)eob;
   /* rows */
   for (i = 0; i < n; i++)
      row1d[tx_size][tx_type](input + i * n, out + i * n);
   /* columns */
   for (i = 0; i < n; i++)
   {
      for (j = 0; j < n; j++) tmpi[j] = out[j * n + i];
      col1d[tx_size][tx_type](tmpi, tmpo);
      for (j = 0; j < n; j++)
         dst[j * stride + i] =
            rvp9_clip8(dst[j * stride + i] + rvp9_rp2(tmpo[j], shift));
   }
}

/* Lossless 4x4 inverse Walsh-Hadamard (vpx iwht4x4_16_add). */
static void rvp9_iwht4x4_add(const rvp9_tran *input, RVP9_PIXEL *dst, int stride)
{
   int i;
   rvp9_tran output[16];
   rvp9_tran a1, b1, c1, d1, e1;
   const rvp9_tran *ip = input;
   rvp9_tran *op = output;

   for (i = 0; i < 4; i++)
   {
      a1 = ip[0] >> 2; c1 = ip[1] >> 2; d1 = ip[2] >> 2; b1 = ip[3] >> 2;
      a1 += c1; d1 -= b1;
      e1 = (a1 - d1) >> 1;
      b1 = e1 - b1; c1 = e1 - c1;
      a1 -= b1; d1 += c1;
      op[0] = a1; op[1] = b1; op[2] = c1; op[3] = d1;
      ip += 4; op += 4;
   }
   ip = output;
   for (i = 0; i < 4; i++)
   {
      a1 = ip[4 * 0]; c1 = ip[4 * 1]; d1 = ip[4 * 2]; b1 = ip[4 * 3];
      a1 += c1; d1 -= b1;
      e1 = (a1 - d1) >> 1;
      b1 = e1 - b1; c1 = e1 - c1;
      a1 -= b1; d1 += c1;
      dst[stride * 0] = rvp9_clip8(dst[stride * 0] + a1);
      dst[stride * 1] = rvp9_clip8(dst[stride * 1] + b1);
      dst[stride * 2] = rvp9_clip8(dst[stride * 2] + c1);
      dst[stride * 3] = rvp9_clip8(dst[stride * 3] + d1);
      ip++; dst++;
   }
}


#endif /* RVP9_RECON_PART == 1 */

#if RVP9_RECON_PART == 2

/* ==================================================================== */
/* Intra edge assembly + prediction dispatch (vp9_reconintra.c).        */
/* ==================================================================== */
/* extend_modes flags */
#define RVP9_NEED_LEFT       1
#define RVP9_NEED_ABOVE      2
#define RVP9_NEED_ABOVERIGHT 4
static const RVP9_PIXEL rvp9_extend_modes[10] = {
   RVP9_NEED_ABOVE | RVP9_NEED_LEFT,   /* DC   */
   RVP9_NEED_ABOVE,                    /* V    */
   RVP9_NEED_LEFT,                     /* H    */
   RVP9_NEED_ABOVERIGHT,               /* D45  */
   RVP9_NEED_LEFT | RVP9_NEED_ABOVE,   /* D135 */
   RVP9_NEED_LEFT | RVP9_NEED_ABOVE,   /* D117 */
   RVP9_NEED_LEFT | RVP9_NEED_ABOVE,   /* D153 */
   RVP9_NEED_LEFT,                     /* D207 */
   RVP9_NEED_ABOVERIGHT,               /* D63  */
   RVP9_NEED_LEFT | RVP9_NEED_ABOVE    /* TM   */
};

static void rvp9_build_intra(rvp9_dec *d, const RVP9_PIXEL *ref, int ref_stride,
      RVP9_PIXEL *dst, int dst_stride, int mode, int tx_size,
      int up_available, int left_available, int right_available,
      int x, int y, int plane, int mi_col, int mi_row)
{
   RVP9_PIXEL left_col[32];
   RVP9_PIXEL above_data[64 + 16];
   RVP9_PIXEL *above_row = above_data + 16;
   const RVP9_PIXEL *const_above_row = above_row;
   const int bs = 4 << tx_size;
   int frame_width  = plane ? d->uvw : d->yw;
   int frame_height = plane ? d->uvh : d->yh;
   int x0, y0, i;
   int ss = plane ? 1 : 0;

   /* mb_to_left/top edge are 0 - mi_col*8*8 etc in libvpx units; the
    * plane-space block origin: */
   x0 = ((mi_col * 8) >> ss) + x;
   y0 = ((mi_row * 8) >> ss) + y;

   if (rvp9_extend_modes[mode] & RVP9_NEED_LEFT)
   {
      if (left_available)
      {
         if (d->mb_to_bottom_edge < 0)
         {
            if (y0 + bs <= frame_height)
               for (i = 0; i < bs; i++) left_col[i] = ref[i * ref_stride - 1];
            else
            {
               const int extend_bottom = frame_height - y0;
               for (i = 0; i < extend_bottom; i++)
                  left_col[i] = ref[i * ref_stride - 1];
               for (; i < bs; i++)
                  left_col[i] = ref[(extend_bottom - 1) * ref_stride - 1];
            }
         }
         else
            for (i = 0; i < bs; i++) left_col[i] = ref[i * ref_stride - 1];
      }
      else
         RVP9_PXFILL(left_col, RVP9_PXMID + 1, bs);
   }

   if (rvp9_extend_modes[mode] & RVP9_NEED_ABOVE)
   {
      if (up_available)
      {
         const RVP9_PIXEL *above_ref = ref - ref_stride;
         if (d->mb_to_right_edge < 0)
         {
            if (x0 + bs <= frame_width)
               RVP9_PXCOPY(above_row, above_ref, bs);
            else if (x0 <= frame_width)
            {
               const int r = frame_width - x0;
               RVP9_PXCOPY(above_row, above_ref, r);
               RVP9_PXFILL(above_row + r, above_row[r - 1], x0 + bs - frame_width);
            }
         }
         else
         {
            if (bs == 4 && right_available && left_available)
               const_above_row = above_ref;
            else
               RVP9_PXCOPY(above_row, above_ref, bs);
         }
         above_row[-1] = left_available ? above_ref[-1] : (RVP9_PXMID + 1);
      }
      else
      {
         RVP9_PXFILL(above_row, RVP9_PXMID - 1, bs);
         above_row[-1] = RVP9_PXMID - 1;
      }
   }

   if (rvp9_extend_modes[mode] & RVP9_NEED_ABOVERIGHT)
   {
      if (up_available)
      {
         const RVP9_PIXEL *above_ref = ref - ref_stride;
         if (d->mb_to_right_edge < 0)
         {
            if (x0 + 2 * bs <= frame_width)
            {
               if (right_available && bs == 4)
                  RVP9_PXCOPY(above_row, above_ref, 2 * bs);
               else
               {
                  RVP9_PXCOPY(above_row, above_ref, bs);
                  RVP9_PXFILL(above_row + bs, above_row[bs - 1], bs);
               }
            }
            else if (x0 + bs <= frame_width)
            {
               const int r = frame_width - x0;
               if (right_available && bs == 4)
               {
                  RVP9_PXCOPY(above_row, above_ref, r);
                  RVP9_PXFILL(above_row + r, above_row[r - 1],
                         x0 + 2 * bs - frame_width);
               }
               else
               {
                  RVP9_PXCOPY(above_row, above_ref, bs);
                  RVP9_PXFILL(above_row + bs, above_row[bs - 1], bs);
               }
            }
            else if (x0 <= frame_width)
            {
               const int r = frame_width - x0;
               RVP9_PXCOPY(above_row, above_ref, r);
               RVP9_PXFILL(above_row + r, above_row[r - 1],
                      x0 + 2 * bs - frame_width);
            }
         }
         else
         {
            if (bs == 4 && right_available && left_available)
               const_above_row = above_ref;
            else
            {
               RVP9_PXCOPY(above_row, above_ref, bs);
               if (bs == 4 && right_available)
                  RVP9_PXCOPY(above_row + bs, above_ref + bs, bs);
               else
                  RVP9_PXFILL(above_row + bs, above_row[bs - 1], bs);
            }
         }
         above_row[-1] = left_available ? above_ref[-1] : (RVP9_PXMID + 1);
      }
      else
      {
         RVP9_PXFILL(above_row, RVP9_PXMID - 1, bs * 2);
         above_row[-1] = RVP9_PXMID - 1;
      }
   }

   /* predict */
   if (mode == RVP9_DC_PRED)
   {
      if (left_available && up_available)
         rvp9_dc_predictor(dst, dst_stride, bs, const_above_row, left_col);
      else if (up_available)
         rvp9_dc_top_predictor(dst, dst_stride, bs, const_above_row, left_col);
      else if (left_available)
         rvp9_dc_left_predictor(dst, dst_stride, bs, const_above_row, left_col);
      else
         rvp9_dc_128_predictor(dst, dst_stride, bs, const_above_row, left_col);
   }
   else
   {
      if (bs == 4 && mode >= 3 && mode <= 8)
      {
         /* specialized 4x4 directional predictors */
         switch (mode)
         {
            case 3: rvp9_d45_predictor_4x4(dst, dst_stride, const_above_row, left_col); break;
            case 4: rvp9_d135_predictor_4x4(dst, dst_stride, const_above_row, left_col); break;
            case 5: rvp9_d117_predictor_4x4(dst, dst_stride, const_above_row, left_col); break;
            case 6: rvp9_d153_predictor_4x4(dst, dst_stride, const_above_row, left_col); break;
            case 7: rvp9_d207_predictor_4x4(dst, dst_stride, const_above_row, left_col); break;
            default: rvp9_d63_predictor_4x4(dst, dst_stride, const_above_row, left_col); break;
         }
      }
      else switch (mode)
      {
         case 1: rvp9_v_predictor(dst, dst_stride, bs, const_above_row, left_col); break;
         case 2: rvp9_h_predictor(dst, dst_stride, bs, const_above_row, left_col); break;
         case 3: rvp9_d45_predictor(dst, dst_stride, bs, const_above_row, left_col); break;
         case 4: rvp9_d135_predictor(dst, dst_stride, bs, const_above_row, left_col); break;
         case 5: rvp9_d117_predictor(dst, dst_stride, bs, const_above_row, left_col); break;
         case 6: rvp9_d153_predictor(dst, dst_stride, bs, const_above_row, left_col); break;
         case 7: rvp9_d207_predictor(dst, dst_stride, bs, const_above_row, left_col); break;
         case 8: rvp9_d63_predictor(dst, dst_stride, bs, const_above_row, left_col); break;
         default: rvp9_tm_predictor(dst, dst_stride, bs, const_above_row, left_col); break;
      }
   }
}

/* ==================================================================== */
/* Mode decode (vp9_decodemv.c, intra frame).                           */
/* ==================================================================== */
static int rvp9_read_intra_mode(rvp9_br *r, const uint8_t *p)
{
   /* vp9_intra_mode_tree walk */
   int i = 0;
   do
   {
      i = rvp9_intra_mode_tree[i + rvp9_br_read(r, p[i >> 1])];
   } while (i > 0);
   return -i;
}

static INLINE int rvp9_get_y_mode(const rvp9_mi *mi, int block)
{
   return mi->sb_type < 3 ? mi->bmodes[block] : mi->mode;
}

static int rvp9_above_block_mode(const rvp9_mi *cur, const rvp9_mi *above,
      int b)
{
   if (b == 0 || b == 1)
      return above ? rvp9_get_y_mode(above, b + 2) : RVP9_DC_PRED;
   return cur->bmodes[b - 2];
}

static int rvp9_left_block_mode(const rvp9_mi *cur, const rvp9_mi *left,
      int b)
{
   if (b == 0 || b == 2)
      return left ? rvp9_get_y_mode(left, b + 1) : RVP9_DC_PRED;
   return cur->bmodes[b - 1];
}

static int rvp9_read_tx_size(rvp9_dec *d, rvp9_mi *mi,
      const rvp9_mi *above, const rvp9_mi *left, rvp9_br *r)
{
   static const uint8_t biggest_tx[5] = { 0, 1, 2, 3, 3 };
   int max_tx = rvp9_max_tx[mi->sb_type];
   if (d->tx_mode == 4 && mi->sb_type >= 3)
   {
      /* get_tx_size_context */
      int above_ctx = (above && !above->skip) ? above->tx_size : max_tx;
      int left_ctx  = (left  && !left->skip)  ? left->tx_size  : max_tx;
      const uint8_t *tp;
      int ctx, tx;
      if (!left)  left_ctx  = above_ctx;
      if (!above) above_ctx = left_ctx;
      ctx = (above_ctx + left_ctx) > max_tx;
      tp = (max_tx == 1) ? d->fc.tx8[ctx] :
           (max_tx == 2) ? d->fc.tx16[ctx] : d->fc.tx32[ctx];
      tx = rvp9_br_read(r, tp[0]);
      if (tx != 0 && max_tx >= 2)
      {
         tx += rvp9_br_read(r, tp[1]);
         if (tx != 1 && max_tx >= 3)
            tx += rvp9_br_read(r, tp[2]);
      }
      if (max_tx == 1)      d->cnt.tx8[ctx][tx]++;
      else if (max_tx == 2) d->cnt.tx16[ctx][tx]++;
      else                  d->cnt.tx32[ctx][tx]++;
      return tx;
   }
   return max_tx < biggest_tx[d->tx_mode] ? max_tx : biggest_tx[d->tx_mode];
}



/* forward declarations (definitions after the loop filter section) */
static void rvp9_build_inter_pred(rvp9_dec *d, int plane, int x, int y,
      int w, int h, const rvp9_mv *mv, const rvp9_fb *ref_fb,
      const int16_t (*kernel)[8], int ref, RVP9_PIXEL *dst_base, int dst_stride,
      int mi_row, int mi_col);
static rvp9_mv rvp9_average_split_mvs(const rvp9_mi *mi, int ref, int ss,
      int block);

/* ==================================================================== */
/* Inter block mode info (read_inter_block_mode_info).                  */
/* ==================================================================== */
static const uint8_t rvp9_size_group[13] = {0,0,0,1,1,1,2,2,2,3,3,3,3};

static int rvp9_read_inter_mode(rvp9_dec *d, rvp9_br *r, int ctx)
{
   int mode = rvp9_read_tree8(r, rvp9_inter_mode_tree,
      d->fc.inter_mode[ctx]);
   d->cnt.inter_mode[ctx][mode]++;
   return RVP9_NEARESTMV + mode;
}

static void rvp9_read_ref_frames(rvp9_dec *d, rvp9_br *r,
      const rvp9_mi *above, const rvp9_mi *left, int8_t ref_frame[2])
{
   int mode;   /* block reference mode */
   if (d->reference_mode == 2)
   {
      int ctx = rvp9_get_reference_mode_context(d, above, left);
      mode = rvp9_br_read(r, d->fc.comp_inter[ctx]);
      d->cnt.comp_inter[ctx][mode]++;
   }
   else
      mode = d->reference_mode;

   if (mode == 1)                               /* COMPOUND */
   {
      const int idx = d->hd.ref_sign_bias[d->comp_fixed_ref];
      const int ctx = rvp9_get_pred_context_comp_ref_p(d, above, left);
      const int bit = rvp9_br_read(r, d->fc.comp_ref[ctx]);
      d->cnt.comp_ref[ctx][bit]++;
      ref_frame[idx]  = (int8_t)d->comp_fixed_ref;
      ref_frame[!idx] = (int8_t)d->comp_var_ref[bit];
   }
   else                                         /* SINGLE */
   {
      const int ctx0 = rvp9_get_pred_context_single_ref_p1(above, left);
      const int bit0 = rvp9_br_read(r, d->fc.single_ref[ctx0][0]);
      d->cnt.single_ref[ctx0][0][bit0]++;
      if (bit0)
      {
         const int ctx1 = rvp9_get_pred_context_single_ref_p2(above, left);
         const int bit1 = rvp9_br_read(r, d->fc.single_ref[ctx1][1]);
         d->cnt.single_ref[ctx1][1][bit1]++;
         ref_frame[0] = bit1 ? 3 : 2;
      }
      else
         ref_frame[0] = 1;
      ref_frame[1] = -1;
   }
}

static int rvp9_get_mode_context(const rvp9_dec *d, const int8_t (*search)[2],
      int mi_row, int mi_col)
{
   /* mode_2_counter over the two nearest; counter_to_context */
   static const uint8_t mode_2_counter[14] =
      { 9,9,9,9,9,9,9,9,9,9, 0,0,3,1 };
   static const uint8_t counter_to_context[19] =
      { 2,3,4,1,3,9,0,9,9,5,5,9,5,9,9,9,9,9,6 };
   int i, counter = 0;
   for (i = 0; i < 2; i++)
   {
      const int8_t *pos = search[i];
      if (rvp9_is_inside(d, mi_col, mi_row, pos))
      {
         const rvp9_mi *cand = d->mi +
            (mi_row + pos[0]) * d->mi_cols + (mi_col + pos[1]);
         counter += mode_2_counter[cand->mode];
      }
   }
   return counter_to_context[counter];
}

/* assign_mv */
static int rvp9_assign_mv(rvp9_dec *d, rvp9_br *r, int mode, rvp9_mv mv[2],
      rvp9_mv ref_mv[2], rvp9_mv near_nearest[2], int is_compound,
      int allow_hp)
{
   int i, ret = 1;
   switch (mode)
   {
      case RVP9_NEWMV:
         for (i = 0; i < 1 + is_compound; i++)
         {
            rvp9_read_mv(d, r, &mv[i], &ref_mv[i], allow_hp);
            ret = ret && rvp9_mv_valid(&mv[i]);
         }
         break;
      case RVP9_NEARMV:
      case RVP9_NEARESTMV:
         mv[0] = near_nearest[0];
         mv[1] = near_nearest[1];
         break;
      case RVP9_ZEROMV:
         mv[0].row = mv[0].col = 0;
         mv[1].row = mv[1].col = 0;
         break;
      default:
         return 0;
   }
   return ret;
}

/* read_inter_block_mode_info */
static int rvp9_read_inter_block_mode(rvp9_dec *d, rvp9_mi *mi,
      const rvp9_mi *above, const rvp9_mi *left, int mi_row, int mi_col,
      rvp9_br *r)
{
   const int bsize = mi->sb_type;
   const int allow_hp = d->hd.allow_high_precision_mv;
   rvp9_mv best_ref[2], best_sub[2];
   int ref, is_compound, mode_ctx;
   const int8_t (*search)[2] = rvp9_mv_ref_blocks[bsize];

   best_ref[0].row = best_ref[0].col = 0;
   best_ref[1].row = best_ref[1].col = 0;

   rvp9_read_ref_frames(d, r, above, left, mi->ref_frame);
   is_compound = rvp9_compound(mi);
   mode_ctx = rvp9_get_mode_context(d, search, mi_row, mi_col);

   if (bsize >= 3)
      mi->mode = (uint8_t)rvp9_read_inter_mode(d, r, mode_ctx);

   if (d->hd.interp_filter == 4)
   {
      int ctx = rvp9_interp_ctx(above, left);
      int f = rvp9_read_tree8(r, rvp9_interp_tree, d->fc.interp[ctx]);
      d->cnt.interp[ctx][f]++;
      mi->interp_filter = (uint8_t)f;
   }
   else
      mi->interp_filter = (uint8_t)d->hd.interp_filter;

   if (bsize < 3)
   {
      int idx, idy, b_mode = 0;
      int got_refs_for_new = 0;
      best_sub[1].row = best_sub[1].col = 0;
      /* NOTE: libvpx sets num_4x4_w = 1 >> !!(partition & VERT):
       * 4x8 -> w=1,h=2; 8x4 -> w=2,h=1; 4x4 -> w=1,h=1 in bmode units
       * where the loops step by num_4x4 over a 2x2 grid. */
      {
         int num_w = (bsize == 1 || bsize == 0) ? 1 : 2;
         int num_h = (bsize == 2 || bsize == 0) ? 1 : 2;
         for (idy = 0; idy < 2; idy += num_h)
            for (idx = 0; idx < 2; idx += num_w)
            {
               const int j = idy * 2 + idx;
               b_mode = rvp9_read_inter_mode(d, r, mode_ctx);

               if (b_mode == RVP9_NEARESTMV || b_mode == RVP9_NEARMV)
               {
                  for (ref = 0; ref < 1 + is_compound; ref++)
                  {
                     /* append_sub8x8_mvs_for_idx */
                     rvp9_mv list[2];
                     int n, cnt2;
                     switch (j)
                     {
                        case 0:
                           cnt2 = rvp9_find_mv_refs(d, mi, b_mode,
                              mi->ref_frame[ref], search, list,
                              mi_row, mi_col, 0);
                           best_sub[ref] = list[cnt2 - 1];
                           break;
                        case 1:
                        case 2:
                           if (b_mode == RVP9_NEARESTMV)
                              best_sub[ref] = mi->bmv[0][ref];
                           else
                           {
                              rvp9_find_mv_refs(d, mi, b_mode,
                                 mi->ref_frame[ref], search, list,
                                 mi_row, mi_col, j);
                              best_sub[ref].row = best_sub[ref].col = 0;
                              for (n = 0; n < 2; n++)
                                 if (!rvp9_mv_eq(mi->bmv[0][ref], list[n]))
                                 {
                                    best_sub[ref] = list[n];
                                    break;
                                 }
                           }
                           break;
                        default:   /* case 3 */
                           if (b_mode == RVP9_NEARESTMV)
                              best_sub[ref] = mi->bmv[2][ref];
                           else
                           {
                              best_sub[ref].row = best_sub[ref].col = 0;
                              if (!rvp9_mv_eq(mi->bmv[2][ref], mi->bmv[1][ref]))
                              {
                                 best_sub[ref] = mi->bmv[1][ref];
                                 break;
                              }
                              if (!rvp9_mv_eq(mi->bmv[2][ref],
                                       mi->bmv[0][ref]))
                              {
                                 best_sub[ref] = mi->bmv[0][ref];
                                 break;
                              }
                              rvp9_find_mv_refs(d, mi, b_mode,
                                 mi->ref_frame[ref], search, list,
                                 mi_row, mi_col, 3);
                              for (n = 0; n < 2; n++)
                                 if (!rvp9_mv_eq(mi->bmv[2][ref], list[n]))
                                 {
                                    best_sub[ref] = list[n];
                                    break;
                                 }
                           }
                           break;
                     }
                  }
               }
               else if (b_mode == RVP9_NEWMV && !got_refs_for_new)
               {
                  for (ref = 0; ref < 1 + is_compound; ref++)
                  {
                     rvp9_mv list[2];
                     rvp9_find_mv_refs(d, mi, RVP9_NEWMV,
                        mi->ref_frame[ref], search, list, mi_row, mi_col, -1);
                     rvp9_lower_mv_precision(&list[0], allow_hp);
                     best_ref[ref] = list[0];
                     got_refs_for_new = 1;
                  }
               }

               if (!rvp9_assign_mv(d, r, b_mode, mi->bmv[j], best_ref,
                     best_sub, is_compound, allow_hp))
                  return 0;

               if (num_h == 2)
               {
                  mi->bmv[j + 2][0] = mi->bmv[j][0];
                  mi->bmv[j + 2][1] = mi->bmv[j][1];
                  mi->bmodes[j + 2] = (uint8_t)b_mode;
               }
               if (num_w == 2)
               {
                  mi->bmv[j + 1][0] = mi->bmv[j][0];
                  mi->bmv[j + 1][1] = mi->bmv[j][1];
                  mi->bmodes[j + 1] = (uint8_t)b_mode;
               }
               mi->bmodes[j] = (uint8_t)b_mode;
            }
      }
      mi->mode = (uint8_t)b_mode;
      mi->mv[0] = mi->bmv[3][0];
      mi->mv[1] = mi->bmv[3][1];
   }
   else
   {
      if (mi->mode != RVP9_ZEROMV)
      {
         for (ref = 0; ref < 1 + is_compound; ref++)
         {
            rvp9_mv list[2];
            int cnt2 = rvp9_find_mv_refs(d, mi, mi->mode,
               mi->ref_frame[ref], search, list, mi_row, mi_col, -1);
            rvp9_lower_mv_precision(&list[cnt2 - 1], allow_hp);
            best_ref[ref] = list[cnt2 - 1];
         }
      }
      if (!rvp9_assign_mv(d, r, mi->mode, mi->mv, best_ref, best_ref,
            is_compound, allow_hp))
         return 0;
      mi->bmv[0][0] = mi->bmv[1][0] = mi->bmv[2][0] = mi->bmv[3][0] = mi->mv[0];
      mi->bmv[0][1] = mi->bmv[1][1] = mi->bmv[2][1] = mi->bmv[3][1] = mi->mv[1];
   }
   return 1;
}

/* ==================================================================== */
/* Block decode: mode info, tokens, reconstruction.                     */
/* ==================================================================== */
static void rvp9_decode_block(rvp9_dec *d, int mi_row, int mi_col,
      int bsize, int bwl, int bhl)
{
   rvp9_mi *mi = d->mi + mi_row * d->mi_cols + mi_col;
   const rvp9_mi *above = mi_row > 0 ? mi - d->mi_cols : NULL;
   /* The left neighbour never crosses a vertical tile edge (libvpx
    * set_mi_row_col: left_mi is gated on tile->mi_col_start); the above
    * neighbour is frame-based even with tile rows. */
   const rvp9_mi *left  = mi_col > d->tile_col_start ? mi - 1 : NULL;
   int bw = 1 << rvp9_mi_w_l2[bsize];
   int bh = 1 << rvp9_mi_h_l2[bsize];
   int x_mis = bw < d->mi_cols - mi_col ? bw : d->mi_cols - mi_col;
   int y_mis = bh < d->mi_rows - mi_row ? bh : d->mi_rows - mi_row;
   int i, j, plane;

   d->mb_to_right_edge  = ((d->mi_cols - bw - mi_col) * 8) * 8;
   d->mb_to_bottom_edge = ((d->mi_rows - bh - mi_row) * 8) * 8;
   d->mb_to_left_edge   = -((mi_col * 8) * 8);
   d->mb_to_top_edge    = -((mi_row * 8) * 8);

   mi->sb_type = (uint8_t)bsize;
   mi->segment_id = 0;                    /* segmentation off in corpus */
   mi->ref_frame[0] = 0;
   mi->ref_frame[1] = -1;
   mi->interp_filter = 3;                 /* SWITCHABLE_FILTERS for intra */
   mi->mv[0].row = mi->mv[0].col = 0;
   mi->mv[1].row = mi->mv[1].col = 0;
   memset(mi->bmv, 0, sizeof mi->bmv);
   {
      /* skip: context from above/left mi */
      int ctx = (above ? above->skip : 0) + (left ? left->skip : 0);
      mi->skip = (uint8_t)rvp9_br_read(&d->r, d->fc.skip[ctx]);
      d->cnt.skip[ctx][mi->skip]++;
   }
   if (d->hd.frame_type == 0 || d->hd.intra_only)
      mi->tx_size = (uint8_t)rvp9_read_tx_size(d, mi, above, left, &d->r);
   else
   {
      int inter_block;
      {
         int ctx = rvp9_intra_inter_ctx(above, left);
         inter_block = rvp9_br_read(&d->r, d->fc.intra_inter[ctx]);
         d->cnt.intra_inter[ctx][inter_block]++;
      }
      /* allow_select = !skip || !inter */
      if ((!mi->skip || !inter_block) && d->tx_mode == 4 && bsize >= 3)
         mi->tx_size = (uint8_t)rvp9_read_tx_size(d, mi, above, left, &d->r);
      else
      {
         static const uint8_t biggest_tx2[5] = { 0, 1, 2, 3, 3 };
         int max_tx = rvp9_max_tx[bsize];
         mi->tx_size = (uint8_t)(max_tx < biggest_tx2[d->tx_mode]
            ? max_tx : biggest_tx2[d->tx_mode]);
      }
      if (inter_block)
      {
         if (!rvp9_read_inter_block_mode(d, mi, above, left,
               mi_row, mi_col, &d->r))
            d->corrupted = 1;
         goto modes_done;
      }
      /* intra block in inter frame: adaptive probs */
      switch (bsize)
      {
         case 0:
            for (i = 0; i < 4; i++)
            {
               int m = rvp9_read_tree8(&d->r, rvp9_intra_mode_tree,
                  d->fc.y_mode[0]);
               d->cnt.y_mode[0][m]++;
               mi->bmodes[i] = (uint8_t)m;
            }
            mi->mode = mi->bmodes[3];
            break;
         case 1:
         {
            int m = rvp9_read_tree8(&d->r, rvp9_intra_mode_tree,
               d->fc.y_mode[0]);
            d->cnt.y_mode[0][m]++;
            mi->bmodes[0] = mi->bmodes[2] = (uint8_t)m;
            m = rvp9_read_tree8(&d->r, rvp9_intra_mode_tree, d->fc.y_mode[0]);
            d->cnt.y_mode[0][m]++;
            mi->bmodes[1] = mi->bmodes[3] = mi->mode = (uint8_t)m;
            break;
         }
         case 2:
         {
            int m = rvp9_read_tree8(&d->r, rvp9_intra_mode_tree,
               d->fc.y_mode[0]);
            d->cnt.y_mode[0][m]++;
            mi->bmodes[0] = mi->bmodes[1] = (uint8_t)m;
            m = rvp9_read_tree8(&d->r, rvp9_intra_mode_tree, d->fc.y_mode[0]);
            d->cnt.y_mode[0][m]++;
            mi->bmodes[2] = mi->bmodes[3] = mi->mode = (uint8_t)m;
            break;
         }
         default:
         {
            int g = rvp9_size_group[bsize];
            int m = rvp9_read_tree8(&d->r, rvp9_intra_mode_tree,
               d->fc.y_mode[g]);
            d->cnt.y_mode[g][m]++;
            mi->mode = (uint8_t)m;
            mi->bmodes[0] = mi->bmodes[1] = mi->bmodes[2] = mi->bmodes[3]
               = mi->mode;
         }
      }
      {
         int m = rvp9_read_tree8(&d->r, rvp9_intra_mode_tree,
            d->fc.uv_mode[mi->mode]);
         d->cnt.uv_mode[mi->mode][m]++;
         mi->uv_mode = (uint8_t)m;
      }
      goto modes_done;
   }

   /* y modes with kf context probs */
   switch (bsize)
   {
      case 0:  /* 4x4 */
         for (i = 0; i < 4; i++)
         {
            int am = rvp9_above_block_mode(mi, above, i);
            int lm = rvp9_left_block_mode(mi, left, i);
            mi->bmodes[i] = (uint8_t)rvp9_read_intra_mode(&d->r,
               rvp9_kf_y_mode_prob + (am * 10 + lm) * 9);
         }
         mi->mode = mi->bmodes[3];
         break;
      case 1:  /* 4x8 */
      {
         int am = rvp9_above_block_mode(mi, above, 0);
         int lm = rvp9_left_block_mode(mi, left, 0);
         mi->bmodes[0] = mi->bmodes[2] = (uint8_t)rvp9_read_intra_mode(&d->r,
            rvp9_kf_y_mode_prob + (am * 10 + lm) * 9);
         am = rvp9_above_block_mode(mi, above, 1);
         lm = rvp9_left_block_mode(mi, left, 1);
         mi->bmodes[1] = mi->bmodes[3] = mi->mode =
            (uint8_t)rvp9_read_intra_mode(&d->r,
               rvp9_kf_y_mode_prob + (am * 10 + lm) * 9);
         break;
      }
      case 2:  /* 8x4 */
      {
         int am = rvp9_above_block_mode(mi, above, 0);
         int lm = rvp9_left_block_mode(mi, left, 0);
         mi->bmodes[0] = mi->bmodes[1] = (uint8_t)rvp9_read_intra_mode(&d->r,
            rvp9_kf_y_mode_prob + (am * 10 + lm) * 9);
         am = rvp9_above_block_mode(mi, above, 2);
         lm = rvp9_left_block_mode(mi, left, 2);
         mi->bmodes[2] = mi->bmodes[3] = mi->mode =
            (uint8_t)rvp9_read_intra_mode(&d->r,
               rvp9_kf_y_mode_prob + (am * 10 + lm) * 9);
         break;
      }
      default:
      {
         int am = rvp9_above_block_mode(mi, above, 0);
         int lm = rvp9_left_block_mode(mi, left, 0);
         mi->mode = (uint8_t)rvp9_read_intra_mode(&d->r,
            rvp9_kf_y_mode_prob + (am * 10 + lm) * 9);
         mi->bmodes[0] = mi->bmodes[1] = mi->bmodes[2] = mi->bmodes[3]
            = mi->mode;
      }
   }
   mi->uv_mode = (uint8_t)rvp9_read_intra_mode(&d->r,
      rvp9_kf_uv_mode_prob + mi->mode * 9);

modes_done:
   /* replicate mi into the covered grid (context for later blocks) */
   for (j = 0; j < y_mis; j++)
      for (i = 0; i < x_mis; i++)
         if (i || j) mi[j * d->mi_cols + i] = *mi;

   /* --- inter blocks: motion compensation over whole block first --- */
   if (rvp9_is_inter(mi))
   {
      const int16_t (*kernel)[8] = rvp9_filter_kernels[mi->interp_filter];
      int is_compound = rvp9_compound(mi);
      int ref;
      for (ref = 0; ref < 1 + is_compound; ref++)
      {
         const rvp9_fb *fb = &d->fbs[d->frame_refs[mi->ref_frame[ref] - 1]];
         for (plane = 0; plane < 3; plane++)
         {
            int ss = plane ? 1 : 0;
            int n4w = 1 << (bwl - ss), n4h = 1 << (bhl - ss);
            RVP9_PIXEL *base;
            int stride;
            if (plane == 0)
            {
               base = (RVP9_PIXEL*)d->buf_y + (mi_row * 8) * d->ys + mi_col * 8;
               stride = d->ys;
            }
            else
            {
               base = (RVP9_PIXEL*)(plane == 1 ? d->buf_u : d->buf_v)
                    + (mi_row * 4) * d->uvs + mi_col * 4;
               stride = d->uvs;
            }
            if (bsize < 3)
            {
               int bx, by, k = 0;
               for (by = 0; by < n4h; by++)
                  for (bx = 0; bx < n4w; bx++)
                  {
                     rvp9_mv mv = rvp9_average_split_mvs(mi, ref, ss, k++);
                     rvp9_build_inter_pred(d, plane, 4 * bx, 4 * by, 4, 4,
                        &mv, fb, kernel, ref, base, stride, mi_row, mi_col);
                  }
            }
            else
               rvp9_build_inter_pred(d, plane, 0, 0, 4 * n4w, 4 * n4h,
                  &mi->mv[ref], fb, kernel, ref, base, stride,
                  mi_row, mi_col);
         }
      }
   }

   /* --- reconstruction over the three planes --- */
   for (plane = 0; plane < 3; plane++)
   {
      int ss = plane ? 1 : 0;
      /* plane block geometry in 4x4 units */
      int n4w_l2 = bwl - ss;
      int n4h_l2 = bhl - ss;
      int num4w = 1 << n4w_l2;
      int num4h = 1 << n4h_l2;
      int tx_size;
      RVP9_PIXEL *base;
      int stride;
      int max_bw, max_bh, step, row, col;

      if (plane == 0)
      {
         tx_size = mi->tx_size;
         base = (RVP9_PIXEL*)d->buf_y + (mi_row * 8) * d->ys + mi_col * 8;
         stride = d->ys;
      }
      else
      {
         /* uv tx size: min(tx_size, max tx of the ss block size);
          * sub-8x8 luma -> 4x4 chroma */
         int ssb = bsize < 3 ? 0 : rvp9_ss_size[bsize];
         int mt = rvp9_max_tx[ssb];
         tx_size = mi->tx_size < mt ? mi->tx_size : mt;
         base = (RVP9_PIXEL*)(plane == 1 ? d->buf_u : d->buf_v)
              + (mi_row * 4) * d->uvs + mi_col * 4;
         stride = d->uvs;
      }

      max_bw = num4w + (d->mb_to_right_edge >= 0 ? 0 :
               d->mb_to_right_edge >> (5 + ss));
      max_bh = num4h + (d->mb_to_bottom_edge >= 0 ? 0 :
               d->mb_to_bottom_edge >> (5 + ss));
      d->max_blocks_wide = d->mb_to_right_edge  >= 0 ? 0 : max_bw;
      d->max_blocks_high = d->mb_to_bottom_edge >= 0 ? 0 : max_bh;

      step = 1 << tx_size;
      for (row = 0; row < max_bh; row += step)
         for (col = 0; col < max_bw; col += step)
         {
            RVP9_PIXEL *dst = base + 4 * row * stride + 4 * col;
            int mode = plane ? mi->uv_mode : mi->mode;
            int inter = rvp9_is_inter(mi);
            int have_top, have_left, have_right;
            if (!inter)
            {
               if (bsize < 3 && plane == 0)
                  mode = mi->bmodes[(row << 1) + col];

               have_top   = row || (mi_row > 0);
               have_left  = col || (mi_col > d->tile_col_start);
               have_right = (col + step) < num4w;

               rvp9_build_intra(d, dst, stride, dst, stride, mode, tx_size,
                  have_top, have_left, have_right, 4 * col, 4 * row, plane,
                  mi_col, mi_row);
            }

            if (!mi->skip)
            {
               int tx_type = (inter || plane || d->hd.lossless) ? 0
                  : rvp9_mode2txtype[mode];
               const int16_t *scan = rvp9_scan_tab(tx_size, tx_type);
               const int16_t *nb   = rvp9_nb_tab(tx_size, tx_type);
               const int16_t *dq   = plane ? d->uv_dq[0] : d->y_dq[0];
               uint8_t *a = d->above_ctx[plane] + ((mi_col * 2) >> ss) + col;
               uint8_t *l = d->left_ctx[plane] + ((mi_row * 2 & 15) >> ss) + row;
               int ctx, eob, k;
               int blocks = 1 << tx_size;
               int csa = 0, csl = 0;   /* edge-truncation shifts */
               if (d->max_blocks_wide && blocks + col > d->max_blocks_wide)
                  csa = blocks - (d->max_blocks_wide - col);
               if (d->max_blocks_high && blocks + row > d->max_blocks_high)
                  csl = blocks - (d->max_blocks_high - row);

               ctx = 0;
               for (k = 0; k < blocks; k++) if (a[k]) { ctx = 1; break; }
               for (k = 0; k < blocks; k++) if (l[k]) { ctx++; break; }

               memset(d->dqcoeff, 0, sizeof(rvp9_tran) << (4 + 2 * tx_size));
               eob = rvp9_decode_coefs(d, plane ? 1 : 0,
                  rvp9_is_inter(mi), d->dqcoeff,
                  tx_size, dq, ctx, scan, nb, &d->r);

               for (k = 0; k < blocks; k++)
                  a[k] = (k < blocks - csa) ? (eob > 0) : 0;
               for (k = 0; k < blocks; k++)
                  l[k] = (k < blocks - csl) ? (eob > 0) : 0;

               if (eob > 0)
               {
                  if (d->hd.lossless)
                     rvp9_iwht4x4_add(d->dqcoeff, dst, stride);
                  else
                     rvp9_iht_add(d->dqcoeff, dst, stride, tx_size,
                        tx_type, eob);
               }
            }
            else
            {
               /* skip: zero the entropy contexts for this tx block */
               int k, blocks = 1 << tx_size;
               uint8_t *a = d->above_ctx[plane] + ((mi_col * 2) >> ss) + col;
               uint8_t *l = d->left_ctx[plane] + ((mi_row * 2 & 15) >> ss) + row;
               for (k = 0; k < blocks; k++) a[k] = 0;
               for (k = 0; k < blocks; k++) l[k] = 0;
            }
         }
   }
}

/* ==================================================================== */
/* Partition recursion (decode_partition).                              */
/* ==================================================================== */
static void rvp9_decode_partition(rvp9_dec *d, int mi_row, int mi_col,
      int bsize, int n4x4_l2)
{
   const int n8x8_l2 = n4x4_l2 - 1;
   const int num_8x8 = 1 << n8x8_l2;
   const int hbs = num_8x8 >> 1;
   int partition, subsize;
   const int has_rows = (mi_row + hbs) < d->mi_rows;
   const int has_cols = (mi_col + hbs) < d->mi_cols;

   if (mi_row >= d->mi_rows || mi_col >= d->mi_cols) return;

   /* read_partition */
   {
      int bsl = n8x8_l2;
      int above = (d->above_seg[mi_col] >> bsl) & 1;
      int lft   = (d->left_seg[mi_row & 7] >> bsl) & 1;
      int ctx = (lft * 2 + above) + bsl * 4;
      int intra = (d->hd.frame_type == 0 || d->hd.intra_only);
      const uint8_t *probs = intra
         ? rvp9_kf_partition_probs + ctx * 3 : d->fc.partition[ctx];
      if (has_rows && has_cols)
      {
         /* partition tree: {-NONE,2, -HORZ,4, -VERT,-SPLIT} */
         if (!rvp9_br_read(&d->r, probs[0]))      partition = 0;
         else if (!rvp9_br_read(&d->r, probs[1])) partition = 1;
         else if (!rvp9_br_read(&d->r, probs[2])) partition = 2;
         else                                     partition = 3;
      }
      else if (!has_rows && has_cols)
         partition = rvp9_br_read(&d->r, probs[1]) ? 3 : 1;
      else if (has_rows && !has_cols)
         partition = rvp9_br_read(&d->r, probs[2]) ? 3 : 2;
      else
         partition = 3;
      d->cnt.partition[ctx][partition]++;
   }
   subsize = rvp9_subsize[partition][bsize];

   if (!hbs)
      rvp9_decode_block(d, mi_row, mi_col, subsize, 1, 1);
   else switch (partition)
   {
      case 0:
         rvp9_decode_block(d, mi_row, mi_col, subsize, n4x4_l2, n4x4_l2);
         break;
      case 1:
         rvp9_decode_block(d, mi_row, mi_col, subsize, n4x4_l2, n8x8_l2);
         if (has_rows)
            rvp9_decode_block(d, mi_row + hbs, mi_col, subsize,
               n4x4_l2, n8x8_l2);
         break;
      case 2:
         rvp9_decode_block(d, mi_row, mi_col, subsize, n8x8_l2, n4x4_l2);
         if (has_cols)
            rvp9_decode_block(d, mi_row, mi_col + hbs, subsize,
               n8x8_l2, n4x4_l2);
         break;
      default:
         rvp9_decode_partition(d, mi_row, mi_col, subsize, n8x8_l2);
         rvp9_decode_partition(d, mi_row, mi_col + hbs, subsize, n8x8_l2);
         rvp9_decode_partition(d, mi_row + hbs, mi_col, subsize, n8x8_l2);
         rvp9_decode_partition(d, mi_row + hbs, mi_col + hbs, subsize,
            n8x8_l2);
         break;
   }

   /* update partition context */
   if (bsize >= 3 && (bsize == 3 || partition != 3))
   {
      memset(d->above_seg + mi_col, rvp9_pctx_above[subsize], num_8x8);
      memset(d->left_seg + (mi_row & 7), rvp9_pctx_left[subsize], num_8x8);
   }
}


#endif /* RVP9_RECON_PART == 2 */

#if RVP9_RECON_PART == 3
static void rvp9_loop_filter_frame(rvp9_dec *d);

static int rvp9_decode_frame_impl(rvp9_dec *d, const uint8_t *data,
      size_t len, int *show_fb)
{
   rvp9_hdr *hd = &d->hd;
   int i, sb_row, sb_col;
   size_t tile_off;


   /* map named ref slots to fbs for inter frames */
   if (hd->frame_type == 1 && !hd->intra_only)
      for (i = 0; i < 3; i++)
      {
         int fb = d->ref_frame_map[hd->ref_idx[i]];
         if (fb < 0 || !d->fbs[fb].valid) return -5;
         d->frame_refs[i] = fb;
         /* only same-size refs supported (no scaling) */
         if (d->fbs[fb].w != hd->w || d->fbs[fb].h != hd->h) return -6;
      }

   d->mi_cols = hd->mi_cols;
   d->mi_rows = hd->mi_rows;
   d->tile_col_start = 0;             /* set per tile before decoding  */
   d->tile_col_end   = d->mi_cols;

   /* allocate on first frame (fixed dims thereafter) */
   if (!d->buf_y)
   {
      size_t ysz, uvsz;
      d->ys  = (d->mi_cols * 8) + 64;
      d->uvs = (d->mi_cols * 4) + 32;
      ysz  = (size_t)d->ys * (d->mi_rows * 8 + 64);
      uvsz = (size_t)d->uvs * (d->mi_rows * 4 + 32);
      for (i = 0; i < RVP9_FRAME_BUFS; i++)
      {
         d->fbs[i].y = (uint8_t*)calloc(sizeof(RVP9_PIXEL), ysz);
         d->fbs[i].u = (uint8_t*)calloc(sizeof(RVP9_PIXEL), uvsz);
         d->fbs[i].v = (uint8_t*)calloc(sizeof(RVP9_PIXEL), uvsz);
         d->fb_bit_depth = RVP9_RECON_HBD ? 10 : 8;
         if (!d->fbs[i].y || !d->fbs[i].u || !d->fbs[i].v) return -7;
      }
      d->mi = (rvp9_mi*)calloc((size_t)d->mi_cols * d->mi_rows,
         sizeof(rvp9_mi));
      d->prev_mvs = (rvp9_mv_ref*)calloc((size_t)d->mi_cols * d->mi_rows,
         sizeof(rvp9_mv_ref));
      d->cur_mvs = (rvp9_mv_ref*)calloc((size_t)d->mi_cols * d->mi_rows,
         sizeof(rvp9_mv_ref));
      d->above_seg = (uint8_t*)calloc(d->mi_cols + 8, 1);
      for (i = 0; i < 3; i++)
         d->above_ctx[i] = (uint8_t*)calloc(d->mi_cols * 2 + 16, 1);
      if (!d->mi || !d->prev_mvs || !d->cur_mvs || !d->above_seg ||
          !d->above_ctx[0] || !d->above_ctx[1] || !d->above_ctx[2])
         return -7;
      for (i = 0; i < 8; i++) d->ref_frame_map[i] = -1;
      d->new_fb = -1;
   }

   /* pick a free frame buffer for this frame */
   {
      int fb = -1;
      for (i = 0; i < RVP9_FRAME_BUFS; i++)
         if (d->fbs[i].ref_count == 0 && i != d->new_fb) { fb = i; break; }
      if (fb < 0) return -9;
      d->new_fb = fb;
      d->fbs[fb].w = hd->w;
      d->fbs[fb].h = hd->h;
      d->buf_y = d->fbs[fb].y;
      d->buf_u = d->fbs[fb].u;
      d->buf_v = d->fbs[fb].v;
   }
   d->yw = hd->w; d->yh = hd->h;
   d->uvw = (hd->w + 1) >> 1;
   d->uvh = (hd->h + 1) >> 1;

   d->use_prev_frame_mvs = !hd->error_resilient &&
      hd->w == d->last_w && hd->h == d->last_h &&
      !d->last_intra_only && d->last_show && !d->last_key &&
      hd->frame_type == 1 && !hd->intra_only;

   if (hd->frame_type == 0 || hd->intra_only || hd->error_resilient)
      rvp9_past_independence(d);
   if (!d->frame_ctxs_init) return -10;

   /* working context */
   d->fc = d->frame_ctxs[hd->frame_context_idx];
   memset(&d->cnt, 0, sizeof d->cnt);
   d->corrupted = 0;

   /* merge LF delta updates into persistent state */
   if (hd->lf_delta_enabled && hd->lf_delta_update)
   {
      for (i = 0; i < 4; i++)
         if (hd->lf_ref_delta_upd[i])
            d->lf_ref_deltas[i] = hd->lf_ref_deltas[i];
      for (i = 0; i < 2; i++)
         if (hd->lf_mode_delta_upd[i])
            d->lf_mode_deltas[i] = hd->lf_mode_deltas[i];
   }

   /* compressed header */
   {
      rvp9_br chr_;
      d->tx_mode = rvp9_parse_compressed_intra(data + hd->uncomp_size,
         hd->first_partition_size, hd, &d->fc, &chr_);
      if (d->tx_mode < 0) return -11;
      if (hd->frame_type == 1 && !hd->intra_only)
      {
         int comp_allowed;
         d->reference_mode = rvp9_parse_compressed_inter(&chr_, hd,
            &d->fc, &comp_allowed);
         if (d->reference_mode != 0)
         {
            /* vp9_setup_compound_reference_mode */
            if (hd->ref_sign_bias[1] == hd->ref_sign_bias[2])
            {
               d->comp_fixed_ref = 3;
               d->comp_var_ref[0] = 1; d->comp_var_ref[1] = 2;
            }
            else if (hd->ref_sign_bias[1] == hd->ref_sign_bias[3])
            {
               d->comp_fixed_ref = 2;
               d->comp_var_ref[0] = 1; d->comp_var_ref[1] = 3;
            }
            else
            {
               d->comp_fixed_ref = 1;
               d->comp_var_ref[0] = 2; d->comp_var_ref[1] = 3;
            }
         }
      }
      else
         d->reference_mode = 0;
   }

   /* dequant: table set and cat6 escape width follow the bit depth
    * (profile 2 streams use the 10/12-bit tables). */
   {
      int ydc = rvp9_clamp(hd->base_qindex + hd->y_dc_delta_q, 0, 255);
      int uvdc = rvp9_clamp(hd->base_qindex + hd->uv_dc_delta_q, 0, 255);
      int uvac = rvp9_clamp(hd->base_qindex + hd->uv_ac_delta_q, 0, 255);
      const int16_t *dcq = rvp9_dc_qlookup;
      const int16_t *acq = rvp9_ac_qlookup;
      d->cat6_prob = rvp9_cat6_prob;
      d->cat6_bits = 14;
      if (hd->bit_depth == 10)
      {
         dcq = rvp9_dc_qlookup_10;
         acq = rvp9_ac_qlookup_10;
         d->cat6_prob = rvp9_cat6_prob_high12 + 2;
         d->cat6_bits = 16;
      }
      else if (hd->bit_depth == 12)
      {
         dcq = rvp9_dc_qlookup_12;
         acq = rvp9_ac_qlookup_12;
         d->cat6_prob = rvp9_cat6_prob_high12;
         d->cat6_bits = 18;
      }
      d->y_dq[0][0]  = dcq[ydc];
      d->y_dq[0][1]  = acq[hd->base_qindex];
      d->uv_dq[0][0] = dcq[uvdc];
      d->uv_dq[0][1] = acq[uvac];
   }


   /* tile data: tiles in raster order; every tile except the last is
    * prefixed with its size as 32-bit big-endian (the last one runs to
    * the end of the frame). Above contexts are cleared once per frame
    * and span tile columns; left contexts are cleared at the start of
    * every superblock row of every tile, which is what confines them
    * to the tile. */
   tile_off = hd->uncomp_size + hd->first_partition_size;
   if (tile_off >= len) return -12;

   memset(d->above_seg, 0, d->mi_cols + 8);
   for (i = 0; i < 3; i++)
      memset(d->above_ctx[i], 0, d->mi_cols * 2 + 16);
   memset(d->mi, 0, (size_t)d->mi_cols * d->mi_rows * sizeof(rvp9_mi));

   {
      const uint8_t *tp   = data + tile_off;
      const uint8_t *tend = data + len;
      int tile_cols       = 1 << hd->log2_tile_cols;
      int tile_rows       = 1 << hd->log2_tile_rows;
      int tr, tc;

      for (tr = 0; tr < tile_rows; tr++)
      {
         int row_start = rvp9_tile_offset(tr,     d->mi_rows,
               hd->log2_tile_rows);
         int row_end   = rvp9_tile_offset(tr + 1, d->mi_rows,
               hd->log2_tile_rows);
         for (tc = 0; tc < tile_cols; tc++)
         {
            int last   = (tr == tile_rows - 1) && (tc == tile_cols - 1);
            size_t tsz;
            if (!last)
            {
               if (tend - tp < 4) return -12;
               tsz = ((size_t)tp[0] << 24) | ((size_t)tp[1] << 16)
                   | ((size_t)tp[2] << 8)  |  (size_t)tp[3];
               tp += 4;
               if (tsz > (size_t)(tend - tp)) return -12;
            }
            else
               tsz = (size_t)(tend - tp);

            d->tile_col_start = rvp9_tile_offset(tc,     d->mi_cols,
                  hd->log2_tile_cols);
            d->tile_col_end   = rvp9_tile_offset(tc + 1, d->mi_cols,
                  hd->log2_tile_cols);

            if (rvp9_br_init(&d->r, tp, tsz)) return -13;

            for (sb_row = row_start; sb_row < row_end; sb_row += 8)
            {
               memset(d->left_seg, 0, sizeof d->left_seg);
               memset(d->left_ctx, 0, sizeof d->left_ctx);
               for (sb_col = d->tile_col_start; sb_col < d->tile_col_end;
                     sb_col += 8)
                  rvp9_decode_partition(d, sb_row, sb_col, 12, 4);
            }
            tp += tsz;
         }
      }
   }
   if (d->corrupted) return -14;

   rvp9_loop_filter_frame(d);

   /* store this frame's MVs for the next frame's prediction */
   {
      rvp9_mv_ref *tmp = d->prev_mvs;
      int n = d->mi_cols * d->mi_rows;
      for (i = 0; i < n; i++)
      {
         d->cur_mvs[i].ref_frame[0] = d->mi[i].ref_frame[0];
         d->cur_mvs[i].ref_frame[1] = d->mi[i].ref_frame[1];
         d->cur_mvs[i].mv[0] = d->mi[i].mv[0];
         d->cur_mvs[i].mv[1] = d->mi[i].mv[1];
      }
      d->prev_mvs = d->cur_mvs;
      d->cur_mvs = tmp;
   }

   /* probability adaptation */
   if (!hd->error_resilient && !hd->frame_parallel_decoding)
   {
      rvp9_adapt_coef_probs(d);
      if (hd->frame_type == 1 && !hd->intra_only)
      {
         rvp9_adapt_mode_probs(d);
         rvp9_adapt_mv_probs(d);
      }
   }

   if (hd->refresh_frame_context)
      d->frame_ctxs[hd->frame_context_idx] = d->fc;

   /* reference slot refresh */
   d->fbs[d->new_fb].valid = 1;
   for (i = 0; i < 8; i++)
      if (hd->refresh_frame_flags & (1 << i))
      {
         int old = d->ref_frame_map[i];
         if (old >= 0) d->fbs[old].ref_count--;
         d->ref_frame_map[i] = d->new_fb;
         d->fbs[d->new_fb].ref_count++;
      }

   d->last_w = hd->w;
   d->last_h = hd->h;
   d->last_show = hd->show_frame;
   d->last_intra_only = hd->intra_only;
   d->last_key = (hd->frame_type == 0);

   if (hd->show_frame) *show_fb = d->new_fb;
   return 0;
}

#endif /* RVP9_RECON_PART == 3 */

#if RVP9_RECON_PART == 4

/* ==================================================================== */
/* Loop filter kernels, ported from vpx_dsp/loopfilter.c (generic C).   */
/* ==================================================================== */
#define ROUND_POWER_OF_TWO(value, n) (((value) + (1 << ((n)-1))) >> (n))
static RVP9_LF_F rvp9_signed_char_clamp(int t) {
  return (RVP9_LF_F)(t < RVP9_LF_CLAMP_MIN ? RVP9_LF_CLAMP_MIN
                   : t > RVP9_LF_CLAMP_MAX ? RVP9_LF_CLAMP_MAX : t);
}

static int8_t rvp9_filter_mask(uint8_t limit, uint8_t blimit, RVP9_PIXEL p3,
                                 RVP9_PIXEL p2, RVP9_PIXEL p1, RVP9_PIXEL p0, RVP9_PIXEL q0,
                                 RVP9_PIXEL q1, RVP9_PIXEL q2, RVP9_PIXEL q3) {
  int8_t mask = 0;
  mask |= (abs(p3 - p2) > RVP9_LF_T(limit)) * -1;
  mask |= (abs(p2 - p1) > RVP9_LF_T(limit)) * -1;
  mask |= (abs(p1 - p0) > RVP9_LF_T(limit)) * -1;
  mask |= (abs(q1 - q0) > RVP9_LF_T(limit)) * -1;
  mask |= (abs(q2 - q1) > RVP9_LF_T(limit)) * -1;
  mask |= (abs(q3 - q2) > RVP9_LF_T(limit)) * -1;
  mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2 > RVP9_LF_T(blimit)) * -1;
  return ~mask;
}

static int8_t rvp9_flat_mask4(uint8_t thresh, RVP9_PIXEL p3, RVP9_PIXEL p2,
                                RVP9_PIXEL p1, RVP9_PIXEL p0, RVP9_PIXEL q0, RVP9_PIXEL q1,
                                RVP9_PIXEL q2, RVP9_PIXEL q3) {
  int8_t mask = 0;
  mask |= (abs(p1 - p0) > RVP9_LF_T(thresh)) * -1;
  mask |= (abs(q1 - q0) > RVP9_LF_T(thresh)) * -1;
  mask |= (abs(p2 - p0) > RVP9_LF_T(thresh)) * -1;
  mask |= (abs(q2 - q0) > RVP9_LF_T(thresh)) * -1;
  mask |= (abs(p3 - p0) > RVP9_LF_T(thresh)) * -1;
  mask |= (abs(q3 - q0) > RVP9_LF_T(thresh)) * -1;
  return ~mask;
}

static int8_t rvp9_flat_mask5(uint8_t thresh, RVP9_PIXEL p4, RVP9_PIXEL p3,
                                RVP9_PIXEL p2, RVP9_PIXEL p1, RVP9_PIXEL p0, RVP9_PIXEL q0,
                                RVP9_PIXEL q1, RVP9_PIXEL q2, RVP9_PIXEL q3,
                                RVP9_PIXEL q4) {
  int8_t mask = ~rvp9_flat_mask4(thresh, p3, p2, p1, p0, q0, q1, q2, q3);
  mask |= (abs(p4 - p0) > RVP9_LF_T(thresh)) * -1;
  mask |= (abs(q4 - q0) > RVP9_LF_T(thresh)) * -1;
  return ~mask;
}

static int8_t rvp9_hev_mask(uint8_t thresh, RVP9_PIXEL p1, RVP9_PIXEL p0,
                              RVP9_PIXEL q0, RVP9_PIXEL q1) {
  int8_t hev = 0;
  hev |= (abs(p1 - p0) > RVP9_LF_T(thresh)) * -1;
  hev |= (abs(q1 - q0) > RVP9_LF_T(thresh)) * -1;
  return hev;
}

static void rvp9_filter4(int8_t mask, uint8_t thresh, RVP9_PIXEL *op1,
                           RVP9_PIXEL *op0, RVP9_PIXEL *oq0, RVP9_PIXEL *oq1) {
  RVP9_LF_F filter1, filter2;

  const RVP9_LF_F ps1 = RVP9_LF_PS(*op1);
  const RVP9_LF_F ps0 = RVP9_LF_PS(*op0);
  const RVP9_LF_F qs0 = RVP9_LF_PS(*oq0);
  const RVP9_LF_F qs1 = RVP9_LF_PS(*oq1);
  const RVP9_LF_F hev = rvp9_hev_mask(thresh, *op1, *op0, *oq0, *oq1);

/* add outer taps if we have high edge variance */
  RVP9_LF_F filter = rvp9_signed_char_clamp(ps1 - qs1) & hev;

/* inner taps */
  filter = rvp9_signed_char_clamp(filter + 3 * (qs0 - ps0)) & mask;

/* save bottom 3 bits so that we round one side +4 and the other +3 */
/* if it equals 4 we'll set it to adjust by -1 to account for the fact */
/* we'd round it by 3 the other way */
  filter1 = rvp9_signed_char_clamp(filter + 4) >> 3;
  filter2 = rvp9_signed_char_clamp(filter + 3) >> 3;

  *oq0 = RVP9_LF_UNPS(qs0 - filter1);
  *op0 = RVP9_LF_UNPS(ps0 + filter2);

/* outer tap adjustments */
  filter = ROUND_POWER_OF_TWO(filter1, 1) & ~hev;

  *oq1 = RVP9_LF_UNPS(qs1 - filter);
  *op1 = RVP9_LF_UNPS(ps1 + filter);
}

static void rvp9_filter8(int8_t mask, uint8_t thresh, int8_t flat,
                           RVP9_PIXEL *op3, RVP9_PIXEL *op2, RVP9_PIXEL *op1,
                           RVP9_PIXEL *op0, RVP9_PIXEL *oq0, RVP9_PIXEL *oq1,
                           RVP9_PIXEL *oq2, RVP9_PIXEL *oq3) {
  if (flat && mask) {
    const RVP9_PIXEL p3 = *op3, p2 = *op2, p1 = *op1, p0 = *op0;
    const RVP9_PIXEL q0 = *oq0, q1 = *oq1, q2 = *oq2, q3 = *oq3;

/* 7-tap filter [1, 1, 1, 2, 1, 1, 1] */
    *op2 = ROUND_POWER_OF_TWO(p3 + p3 + p3 + 2 * p2 + p1 + p0 + q0, 3);
    *op1 = ROUND_POWER_OF_TWO(p3 + p3 + p2 + 2 * p1 + p0 + q0 + q1, 3);
    *op0 = ROUND_POWER_OF_TWO(p3 + p2 + p1 + 2 * p0 + q0 + q1 + q2, 3);
    *oq0 = ROUND_POWER_OF_TWO(p2 + p1 + p0 + 2 * q0 + q1 + q2 + q3, 3);
    *oq1 = ROUND_POWER_OF_TWO(p1 + p0 + q0 + 2 * q1 + q2 + q3 + q3, 3);
    *oq2 = ROUND_POWER_OF_TWO(p0 + q0 + q1 + 2 * q2 + q3 + q3 + q3, 3);
  } else {
    rvp9_filter4(mask, thresh, op1, op0, oq0, oq1);
  }
}

static void rvp9_filter16(int8_t mask, uint8_t thresh, int8_t flat,
                            int8_t flat2, RVP9_PIXEL *op7, RVP9_PIXEL *op6,
                            RVP9_PIXEL *op5, RVP9_PIXEL *op4, RVP9_PIXEL *op3,
                            RVP9_PIXEL *op2, RVP9_PIXEL *op1, RVP9_PIXEL *op0,
                            RVP9_PIXEL *oq0, RVP9_PIXEL *oq1, RVP9_PIXEL *oq2,
                            RVP9_PIXEL *oq3, RVP9_PIXEL *oq4, RVP9_PIXEL *oq5,
                            RVP9_PIXEL *oq6, RVP9_PIXEL *oq7) {
  if (flat2 && flat && mask) {
    const RVP9_PIXEL p7 = *op7, p6 = *op6, p5 = *op5, p4 = *op4, p3 = *op3,
                  p2 = *op2, p1 = *op1, p0 = *op0;

    const RVP9_PIXEL q0 = *oq0, q1 = *oq1, q2 = *oq2, q3 = *oq3, q4 = *oq4,
                  q5 = *oq5, q6 = *oq6, q7 = *oq7;

/* 15-tap filter [1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1] */
    *op6 = ROUND_POWER_OF_TWO(
        p7 * 7 + p6 * 2 + p5 + p4 + p3 + p2 + p1 + p0 + q0, 4);
    *op5 = ROUND_POWER_OF_TWO(
        p7 * 6 + p6 + p5 * 2 + p4 + p3 + p2 + p1 + p0 + q0 + q1, 4);
    *op4 = ROUND_POWER_OF_TWO(
        p7 * 5 + p6 + p5 + p4 * 2 + p3 + p2 + p1 + p0 + q0 + q1 + q2, 4);
    *op3 = ROUND_POWER_OF_TWO(
        p7 * 4 + p6 + p5 + p4 + p3 * 2 + p2 + p1 + p0 + q0 + q1 + q2 + q3, 4);
    *op2 = ROUND_POWER_OF_TWO(
        p7 * 3 + p6 + p5 + p4 + p3 + p2 * 2 + p1 + p0 + q0 + q1 + q2 + q3 + q4,
        4);
    *op1 = ROUND_POWER_OF_TWO(p7 * 2 + p6 + p5 + p4 + p3 + p2 + p1 * 2 + p0 +
                                  q0 + q1 + q2 + q3 + q4 + q5,
                              4);
    *op0 = ROUND_POWER_OF_TWO(p7 + p6 + p5 + p4 + p3 + p2 + p1 + p0 * 2 + q0 +
                                  q1 + q2 + q3 + q4 + q5 + q6,
                              4);
    *oq0 = ROUND_POWER_OF_TWO(p6 + p5 + p4 + p3 + p2 + p1 + p0 + q0 * 2 + q1 +
                                  q2 + q3 + q4 + q5 + q6 + q7,
                              4);
    *oq1 = ROUND_POWER_OF_TWO(p5 + p4 + p3 + p2 + p1 + p0 + q0 + q1 * 2 + q2 +
                                  q3 + q4 + q5 + q6 + q7 * 2,
                              4);
    *oq2 = ROUND_POWER_OF_TWO(
        p4 + p3 + p2 + p1 + p0 + q0 + q1 + q2 * 2 + q3 + q4 + q5 + q6 + q7 * 3,
        4);
    *oq3 = ROUND_POWER_OF_TWO(
        p3 + p2 + p1 + p0 + q0 + q1 + q2 + q3 * 2 + q4 + q5 + q6 + q7 * 4, 4);
    *oq4 = ROUND_POWER_OF_TWO(
        p2 + p1 + p0 + q0 + q1 + q2 + q3 + q4 * 2 + q5 + q6 + q7 * 5, 4);
    *oq5 = ROUND_POWER_OF_TWO(
        p1 + p0 + q0 + q1 + q2 + q3 + q4 + q5 * 2 + q6 + q7 * 6, 4);
    *oq6 = ROUND_POWER_OF_TWO(
        p0 + q0 + q1 + q2 + q3 + q4 + q5 + q6 * 2 + q7 * 7, 4);
  } else {
    rvp9_filter8(mask, thresh, flat, op3, op2, op1, op0, oq0, oq1, oq2, oq3);
  }
}

#if defined(RVP9_SSE2) && !RVP9_RECON_HBD
/* ---- 8-lane SSE2 loop filter ----
 * Each __m128i carries 8 samples (one per filtered line) in its low
 * half. Every step mirrors the scalar functions above exactly:
 * - the six 'limit' terms use max-of-absdiffs (max(a,b) > L is
 *   identical to (a > L) | (b > L)), compared via subs_epu8 == 0,
 *   which is exact for all u8 inputs;
 * - the blimit term (abs(p0-q0)*2 + abs(p1-q1)/2) is evaluated in
 *   16-bit lanes so no saturation can occur;
 * - filter4 runs in saturating int8 (adds/subs_epi8 ==
 *   signed_char_clamp); the three successive saturating adds of
 *   (qs0 - ps0) equal the scalar's single clamp of filter + 3*d for
 *   all inputs (when |d| <= 127 no intermediate can saturate because
 *   the partial sums lie between two in-range endpoints; when |d| >=
 *   128 the true result is already at the rail the saturated steps
 *   reach);
 * - filter8/filter16 smoothing runs in u16 lanes (sums <= 4080) with
 *   the +4/+8 rounding and shifts of the scalar, packed back with
 *   packus (values are exact 8-bit results, so no saturation);
 * - lanes where a level's condition fails keep the previous level's
 *   result via mask blends, reproducing the scalar cascade
 *   filter16 -> filter8 -> filter4 -> unchanged. */

static INLINE __m128i rvp9_lf_absd8(__m128i a, __m128i b)
{
   return _mm_or_si128(_mm_subs_epu8(a, b), _mm_subs_epu8(b, a));
}

/* 0xFF lanes where max(a..) <= t */
static INLINE __m128i rvp9_lf_le8(__m128i maxv, __m128i t)
{
   return _mm_cmpeq_epi8(_mm_subs_epu8(maxv, t), _mm_setzero_si128());
}

static INLINE __m128i rvp9_lf_blend8(__m128i sel, __m128i a, __m128i b)
{
   /* sel ? a : b, sel lanes are 0x00/0xFF */
   return _mm_or_si128(_mm_and_si128(sel, a), _mm_andnot_si128(sel, b));
}

/* filter_mask (0xFF = filter) and hev (0xFF = high edge variance) */
static void rvp9_lf_mask_hev_sse2(__m128i limit, __m128i blimit,
      __m128i thresh,
      __m128i p3, __m128i p2, __m128i p1, __m128i p0,
      __m128i q0, __m128i q1, __m128i q2, __m128i q3,
      __m128i *mask, __m128i *hev)
{
   const __m128i zero = _mm_setzero_si128();
   __m128i m = _mm_max_epu8(rvp9_lf_absd8(p3, p2), rvp9_lf_absd8(p2, p1));
   __m128i in_limit, in_blimit;
   __m128i d0, d1, t_lo, blim_lo;
   m = _mm_max_epu8(m, rvp9_lf_absd8(p1, p0));
   m = _mm_max_epu8(m, rvp9_lf_absd8(q1, q0));
   m = _mm_max_epu8(m, rvp9_lf_absd8(q2, q1));
   m = _mm_max_epu8(m, rvp9_lf_absd8(q3, q2));
   in_limit = rvp9_lf_le8(m, limit);

   /* abs(p0-q0)*2 + abs(p1-q1)/2 <= blimit, exact in 16-bit */
   d0      = rvp9_lf_absd8(p0, q0);
   d1      = rvp9_lf_absd8(p1, q1);
   t_lo    = _mm_add_epi16(
         _mm_slli_epi16(_mm_unpacklo_epi8(d0, zero), 1),
         _mm_srli_epi16(_mm_unpacklo_epi8(d1, zero), 1));
   blim_lo = _mm_unpacklo_epi8(blimit, zero);
   in_blimit = _mm_packs_epi16(
         _mm_cmpgt_epi16(t_lo, blim_lo), _mm_cmpgt_epi16(t_lo, blim_lo));
   in_blimit = _mm_andnot_si128(in_blimit, _mm_set1_epi8((char)0xFF));

   *mask = _mm_and_si128(in_limit, in_blimit);

   *hev  = _mm_xor_si128(
         rvp9_lf_le8(_mm_max_epu8(rvp9_lf_absd8(p1, p0),
                                  rvp9_lf_absd8(q1, q0)), thresh),
         _mm_set1_epi8((char)0xFF));
}

/* flat_mask4 with thresh 1 (0xFF = flat) */
static INLINE __m128i rvp9_lf_flat4_sse2(__m128i p3, __m128i p2,
      __m128i p1, __m128i p0, __m128i q0, __m128i q1, __m128i q2,
      __m128i q3)
{
   const __m128i one = _mm_set1_epi8(1);
   __m128i m = _mm_max_epu8(rvp9_lf_absd8(p1, p0), rvp9_lf_absd8(q1, q0));
   m = _mm_max_epu8(m, rvp9_lf_absd8(p2, p0));
   m = _mm_max_epu8(m, rvp9_lf_absd8(q2, q0));
   m = _mm_max_epu8(m, rvp9_lf_absd8(p3, p0));
   m = _mm_max_epu8(m, rvp9_lf_absd8(q3, q0));
   return rvp9_lf_le8(m, one);
}

/* filter4 on 8 lanes; writes new p1,p0,q0,q1 */
static void rvp9_lf_filter4_sse2(__m128i mask, __m128i hev,
      __m128i *p1, __m128i *p0, __m128i *q0, __m128i *q1)
{
   const __m128i zero = _mm_setzero_si128();
   const __m128i sb   = _mm_set1_epi8((char)0x80);
   const __m128i t3   = _mm_set1_epi8(3);
   const __m128i t4   = _mm_set1_epi8(4);
   const __m128i one  = _mm_set1_epi8(1);
   __m128i ps1 = _mm_xor_si128(*p1, sb);
   __m128i ps0 = _mm_xor_si128(*p0, sb);
   __m128i qs0 = _mm_xor_si128(*q0, sb);
   __m128i qs1 = _mm_xor_si128(*q1, sb);
   __m128i filt, work, f1, f2, f3;

   filt = _mm_and_si128(_mm_subs_epi8(ps1, qs1), hev);
   work = _mm_subs_epi8(qs0, ps0);
   filt = _mm_adds_epi8(filt, work);
   filt = _mm_adds_epi8(filt, work);
   filt = _mm_adds_epi8(filt, work);
   filt = _mm_and_si128(filt, mask);

   f1 = _mm_adds_epi8(filt, t4);
   f2 = _mm_adds_epi8(filt, t3);
   /* arithmetic >>3 on int8 lanes: value into the high byte, srai 11 */
   f1 = _mm_packs_epi16(
         _mm_srai_epi16(_mm_unpacklo_epi8(zero, f1), 11),
         _mm_srai_epi16(_mm_unpackhi_epi8(zero, f1), 11));
   f2 = _mm_packs_epi16(
         _mm_srai_epi16(_mm_unpacklo_epi8(zero, f2), 11),
         _mm_srai_epi16(_mm_unpackhi_epi8(zero, f2), 11));

   *q0 = _mm_xor_si128(_mm_subs_epi8(qs0, f1), sb);
   *p0 = _mm_xor_si128(_mm_adds_epi8(ps0, f2), sb);

   /* (f1 + 1) >> 1, masked off where hev */
   f3 = _mm_adds_epi8(f1, one);
   f3 = _mm_packs_epi16(
         _mm_srai_epi16(_mm_unpacklo_epi8(zero, f3), 9),
         _mm_srai_epi16(_mm_unpackhi_epi8(zero, f3), 9));
   f3 = _mm_andnot_si128(hev, f3);

   *q1 = _mm_xor_si128(_mm_subs_epi8(qs1, f3), sb);
   *p1 = _mm_xor_si128(_mm_adds_epi8(ps1, f3), sb);
}

/* One 7-tap smoothing output in u16 lanes: (sum + 4) >> 3 packed */
static INLINE __m128i rvp9_lf_r3(__m128i sum_lo)
{
   __m128i r = _mm_srli_epi16(_mm_add_epi16(sum_lo, _mm_set1_epi16(4)), 3);
   return _mm_packus_epi16(r, r);
}

static INLINE __m128i rvp9_lf_r4(__m128i sum_lo)
{
   __m128i r = _mm_srli_epi16(_mm_add_epi16(sum_lo, _mm_set1_epi16(8)), 4);
   return _mm_packus_epi16(r, r);
}

/* filter8 cascade on 8 lanes: filter4 everywhere, 7-tap where
 * (flat & mask). p3/q3 are read-only. */
static void rvp9_lf_filter8_sse2(__m128i mask, __m128i hev, __m128i flat,
      __m128i p3, __m128i *p2, __m128i *p1, __m128i *p0,
      __m128i *q0, __m128i *q1, __m128i *q2, __m128i q3)
{
   const __m128i zero = _mm_setzero_si128();
   __m128i sel = _mm_and_si128(flat, mask);
   __m128i f_p1 = *p1, f_p0 = *p0, f_q0 = *q0, f_q1 = *q1;
   __m128i P3 = _mm_unpacklo_epi8(p3, zero);
   __m128i P2 = _mm_unpacklo_epi8(*p2, zero);
   __m128i P1 = _mm_unpacklo_epi8(*p1, zero);
   __m128i P0 = _mm_unpacklo_epi8(*p0, zero);
   __m128i Q0 = _mm_unpacklo_epi8(*q0, zero);
   __m128i Q1 = _mm_unpacklo_epi8(*q1, zero);
   __m128i Q2 = _mm_unpacklo_epi8(*q2, zero);
   __m128i Q3 = _mm_unpacklo_epi8(q3, zero);
   __m128i sum, s_p2, s_p1, s_p0, s_q0, s_q1, s_q2;

   rvp9_lf_filter4_sse2(mask, hev, &f_p1, &f_p0, &f_q0, &f_q1);

   /* op2 = (p3+p3+p3+2p2+p1+p0+q0 + 4) >> 3, then slide */
   sum  = _mm_add_epi16(_mm_add_epi16(P3, P3),
          _mm_add_epi16(_mm_add_epi16(P3, P2),
          _mm_add_epi16(_mm_add_epi16(P2, P1),
                        _mm_add_epi16(P0, Q0))));
   s_p2 = rvp9_lf_r3(sum);
   sum  = _mm_add_epi16(sum, _mm_sub_epi16(
          _mm_add_epi16(P1, Q1), _mm_add_epi16(P3, P2)));
   s_p1 = rvp9_lf_r3(sum);
   sum  = _mm_add_epi16(sum, _mm_sub_epi16(
          _mm_add_epi16(P0, Q2), _mm_add_epi16(P3, P1)));
   s_p0 = rvp9_lf_r3(sum);
   sum  = _mm_add_epi16(sum, _mm_sub_epi16(
          _mm_add_epi16(Q0, Q3), _mm_add_epi16(P3, P0)));
   s_q0 = rvp9_lf_r3(sum);
   sum  = _mm_add_epi16(sum, _mm_sub_epi16(
          _mm_add_epi16(Q1, Q3), _mm_add_epi16(P2, Q0)));
   s_q1 = rvp9_lf_r3(sum);
   sum  = _mm_add_epi16(sum, _mm_sub_epi16(
          _mm_add_epi16(Q2, Q3), _mm_add_epi16(P1, Q1)));
   s_q2 = rvp9_lf_r3(sum);

   *p2 = rvp9_lf_blend8(sel, s_p2, *p2);
   *p1 = rvp9_lf_blend8(sel, s_p1, f_p1);
   *p0 = rvp9_lf_blend8(sel, s_p0, f_p0);
   *q0 = rvp9_lf_blend8(sel, s_q0, f_q0);
   *q1 = rvp9_lf_blend8(sel, s_q1, f_q1);
   *q2 = rvp9_lf_blend8(sel, s_q2, *q2);
}

/* full 16 cascade on 8 lanes; r[0]=p7 .. r[7]=p0, r[8]=q0 .. r[15]=q7;
 * r[0]/r[15] are read-only. The 15-tap smoothing consumes ORIGINAL
 * samples (in the scalar, filter8/filter4 only run in the else
 * branch), so the samples are widened before the inner cascade
 * mutates r[5..10]; per-lane selection then reproduces the cascade. */
static void rvp9_lf_filter16_sse2(__m128i mask, __m128i hev, __m128i flat,
      __m128i flat2, __m128i r[16])
{
   const __m128i zero = _mm_setzero_si128();
   __m128i sel = _mm_and_si128(flat2, _mm_and_si128(flat, mask));
   __m128i W[16], out[14], sum;
   int i;

   for (i = 0; i < 16; i++)
      W[i] = _mm_unpacklo_epi8(r[i], zero);

   /* filter8 (which itself runs filter4) on the inner samples */
   rvp9_lf_filter8_sse2(mask, hev, flat, r[4], &r[5], &r[6], &r[7],
         &r[8], &r[9], &r[10], r[11]);

   /* 15-tap [1,1,1,1,1,1,1,2,1,1,1,1,1,1,1] as a sliding sum:
    * S(p6) = 7*p7 + 2*p6 + p5+p4+p3+p2+p1+p0+q0 */
   sum = _mm_add_epi16(_mm_slli_epi16(W[0], 3), W[1]); /* 8p7 + p6 */
   sum = _mm_sub_epi16(sum, W[0]);                     /* 7p7 + p6 */
   sum = _mm_add_epi16(sum, W[1]);                     /* + p6     */
   for (i = 2; i <= 8; i++)
      sum = _mm_add_epi16(sum, W[i]);
   out[0] = rvp9_lf_r4(sum);                           /* p6 */
   /* p-side slides: - p7 - prev_center + new_center + next q */
   sum = _mm_add_epi16(sum, _mm_sub_epi16(
         _mm_add_epi16(W[2], W[9]),  _mm_add_epi16(W[0], W[1])));
   out[1] = rvp9_lf_r4(sum);                           /* p5 */
   sum = _mm_add_epi16(sum, _mm_sub_epi16(
         _mm_add_epi16(W[3], W[10]), _mm_add_epi16(W[0], W[2])));
   out[2] = rvp9_lf_r4(sum);                           /* p4 */
   sum = _mm_add_epi16(sum, _mm_sub_epi16(
         _mm_add_epi16(W[4], W[11]), _mm_add_epi16(W[0], W[3])));
   out[3] = rvp9_lf_r4(sum);                           /* p3 */
   sum = _mm_add_epi16(sum, _mm_sub_epi16(
         _mm_add_epi16(W[5], W[12]), _mm_add_epi16(W[0], W[4])));
   out[4] = rvp9_lf_r4(sum);                           /* p2 */
   sum = _mm_add_epi16(sum, _mm_sub_epi16(
         _mm_add_epi16(W[6], W[13]), _mm_add_epi16(W[0], W[5])));
   out[5] = rvp9_lf_r4(sum);                           /* p1 */
   sum = _mm_add_epi16(sum, _mm_sub_epi16(
         _mm_add_epi16(W[7], W[14]), _mm_add_epi16(W[0], W[6])));
   out[6] = rvp9_lf_r4(sum);                           /* p0 */
   sum = _mm_add_epi16(sum, _mm_sub_epi16(
         _mm_add_epi16(W[8], W[15]), _mm_add_epi16(W[0], W[7])));
   out[7] = rvp9_lf_r4(sum);                           /* q0 */
   /* q-side slides: - leaving p - prev_center + new_center + q7 */
   sum = _mm_add_epi16(sum, _mm_sub_epi16(
         _mm_add_epi16(W[9],  W[15]), _mm_add_epi16(W[1], W[8])));
   out[8] = rvp9_lf_r4(sum);                           /* q1 */
   sum = _mm_add_epi16(sum, _mm_sub_epi16(
         _mm_add_epi16(W[10], W[15]), _mm_add_epi16(W[2], W[9])));
   out[9] = rvp9_lf_r4(sum);                           /* q2 */
   sum = _mm_add_epi16(sum, _mm_sub_epi16(
         _mm_add_epi16(W[11], W[15]), _mm_add_epi16(W[3], W[10])));
   out[10] = rvp9_lf_r4(sum);                          /* q3 */
   sum = _mm_add_epi16(sum, _mm_sub_epi16(
         _mm_add_epi16(W[12], W[15]), _mm_add_epi16(W[4], W[11])));
   out[11] = rvp9_lf_r4(sum);                          /* q4 */
   sum = _mm_add_epi16(sum, _mm_sub_epi16(
         _mm_add_epi16(W[13], W[15]), _mm_add_epi16(W[5], W[12])));
   out[12] = rvp9_lf_r4(sum);                          /* q5 */
   sum = _mm_add_epi16(sum, _mm_sub_epi16(
         _mm_add_epi16(W[14], W[15]), _mm_add_epi16(W[6], W[13])));
   out[13] = rvp9_lf_r4(sum);                          /* q6 */

   for (i = 0; i < 14; i++)
      r[i + 1] = rvp9_lf_blend8(sel, out[i], r[i + 1]);
}

/* 8x8 byte transpose: in[k] low 8 bytes are row k; out likewise. */
static void rvp9_lf_transpose8x8(const __m128i *in, __m128i *out)
{
   __m128i a0 = _mm_unpacklo_epi8(in[0], in[1]);
   __m128i a1 = _mm_unpacklo_epi8(in[2], in[3]);
   __m128i a2 = _mm_unpacklo_epi8(in[4], in[5]);
   __m128i a3 = _mm_unpacklo_epi8(in[6], in[7]);
   __m128i b0 = _mm_unpacklo_epi16(a0, a1);
   __m128i b1 = _mm_unpackhi_epi16(a0, a1);
   __m128i b2 = _mm_unpacklo_epi16(a2, a3);
   __m128i b3 = _mm_unpackhi_epi16(a2, a3);
   __m128i c0 = _mm_unpacklo_epi32(b0, b2);
   __m128i c1 = _mm_unpackhi_epi32(b0, b2);
   __m128i c2 = _mm_unpacklo_epi32(b1, b3);
   __m128i c3 = _mm_unpackhi_epi32(b1, b3);
   out[0] = c0;
   out[1] = _mm_srli_si128(c0, 8);
   out[2] = c1;
   out[3] = _mm_srli_si128(c1, 8);
   out[4] = c2;
   out[5] = _mm_srli_si128(c2, 8);
   out[6] = c3;
   out[7] = _mm_srli_si128(c3, 8);
}



/* ---- SSE2 edge appliers (8 lines per call, like the scalar) ---- */

static void rvp9_lpf_horizontal_4_sse2(uint8_t *s, int pitch,
      uint8_t blimit, uint8_t limit, uint8_t thresh)
{
   __m128i bl = _mm_set1_epi8((char)blimit);
   __m128i li = _mm_set1_epi8((char)limit);
   __m128i th = _mm_set1_epi8((char)thresh);
   __m128i p3 = _mm_loadl_epi64((const __m128i*)(s - 4 * pitch));
   __m128i p2 = _mm_loadl_epi64((const __m128i*)(s - 3 * pitch));
   __m128i p1 = _mm_loadl_epi64((const __m128i*)(s - 2 * pitch));
   __m128i p0 = _mm_loadl_epi64((const __m128i*)(s - 1 * pitch));
   __m128i q0 = _mm_loadl_epi64((const __m128i*)(s + 0 * pitch));
   __m128i q1 = _mm_loadl_epi64((const __m128i*)(s + 1 * pitch));
   __m128i q2 = _mm_loadl_epi64((const __m128i*)(s + 2 * pitch));
   __m128i q3 = _mm_loadl_epi64((const __m128i*)(s + 3 * pitch));
   __m128i mask, hev;
   rvp9_lf_mask_hev_sse2(li, bl, th, p3, p2, p1, p0, q0, q1, q2, q3,
         &mask, &hev);
   rvp9_lf_filter4_sse2(mask, hev, &p1, &p0, &q0, &q1);
   _mm_storel_epi64((__m128i*)(s - 2 * pitch), p1);
   _mm_storel_epi64((__m128i*)(s - 1 * pitch), p0);
   _mm_storel_epi64((__m128i*)(s + 0 * pitch), q0);
   _mm_storel_epi64((__m128i*)(s + 1 * pitch), q1);
}

static void rvp9_lpf_horizontal_8_sse2(uint8_t *s, int pitch,
      uint8_t blimit, uint8_t limit, uint8_t thresh)
{
   __m128i bl = _mm_set1_epi8((char)blimit);
   __m128i li = _mm_set1_epi8((char)limit);
   __m128i th = _mm_set1_epi8((char)thresh);
   __m128i p3 = _mm_loadl_epi64((const __m128i*)(s - 4 * pitch));
   __m128i p2 = _mm_loadl_epi64((const __m128i*)(s - 3 * pitch));
   __m128i p1 = _mm_loadl_epi64((const __m128i*)(s - 2 * pitch));
   __m128i p0 = _mm_loadl_epi64((const __m128i*)(s - 1 * pitch));
   __m128i q0 = _mm_loadl_epi64((const __m128i*)(s + 0 * pitch));
   __m128i q1 = _mm_loadl_epi64((const __m128i*)(s + 1 * pitch));
   __m128i q2 = _mm_loadl_epi64((const __m128i*)(s + 2 * pitch));
   __m128i q3 = _mm_loadl_epi64((const __m128i*)(s + 3 * pitch));
   __m128i mask, hev, flat;
   rvp9_lf_mask_hev_sse2(li, bl, th, p3, p2, p1, p0, q0, q1, q2, q3,
         &mask, &hev);
   flat = rvp9_lf_flat4_sse2(p3, p2, p1, p0, q0, q1, q2, q3);
   rvp9_lf_filter8_sse2(mask, hev, flat, p3, &p2, &p1, &p0,
         &q0, &q1, &q2, q3);
   _mm_storel_epi64((__m128i*)(s - 3 * pitch), p2);
   _mm_storel_epi64((__m128i*)(s - 2 * pitch), p1);
   _mm_storel_epi64((__m128i*)(s - 1 * pitch), p0);
   _mm_storel_epi64((__m128i*)(s + 0 * pitch), q0);
   _mm_storel_epi64((__m128i*)(s + 1 * pitch), q1);
   _mm_storel_epi64((__m128i*)(s + 2 * pitch), q2);
}

static void rvp9_lpf_horizontal_16_sse2(uint8_t *s, int pitch,
      uint8_t blimit, uint8_t limit, uint8_t thresh)
{
   __m128i bl = _mm_set1_epi8((char)blimit);
   __m128i li = _mm_set1_epi8((char)limit);
   __m128i th = _mm_set1_epi8((char)thresh);
   __m128i r[16], mask, hev, flat, flat2, m5;
   int k;
   for (k = 0; k < 16; k++)
      r[k] = _mm_loadl_epi64((const __m128i*)(s + (k - 8) * pitch));
   rvp9_lf_mask_hev_sse2(li, bl, th, r[4], r[5], r[6], r[7],
         r[8], r[9], r[10], r[11], &mask, &hev);
   flat = rvp9_lf_flat4_sse2(r[4], r[5], r[6], r[7],
         r[8], r[9], r[10], r[11]);
   m5 = _mm_max_epu8(rvp9_lf_absd8(r[0], r[7]), rvp9_lf_absd8(r[15], r[8]));
   m5 = _mm_max_epu8(m5, rvp9_lf_absd8(r[1], r[7]));
   m5 = _mm_max_epu8(m5, rvp9_lf_absd8(r[14], r[8]));
   m5 = _mm_max_epu8(m5, rvp9_lf_absd8(r[2], r[7]));
   m5 = _mm_max_epu8(m5, rvp9_lf_absd8(r[13], r[8]));
   m5 = _mm_max_epu8(m5, rvp9_lf_absd8(r[3], r[7]));
   m5 = _mm_max_epu8(m5, rvp9_lf_absd8(r[12], r[8]));
   flat2 = rvp9_lf_le8(m5, _mm_set1_epi8(1));
   rvp9_lf_filter16_sse2(mask, hev, flat, flat2, r);
   for (k = 1; k < 15; k++)
      _mm_storel_epi64((__m128i*)(s + (k - 8) * pitch), r[k]);
}

static void rvp9_lpf_vertical_4_sse2(uint8_t *s, int pitch,
      uint8_t blimit, uint8_t limit, uint8_t thresh)
{
   __m128i bl = _mm_set1_epi8((char)blimit);
   __m128i li = _mm_set1_epi8((char)limit);
   __m128i th = _mm_set1_epi8((char)thresh);
   __m128i rows[8], cols[8], mask, hev;
   int k;
   for (k = 0; k < 8; k++)
      rows[k] = _mm_loadl_epi64((const __m128i*)(s - 4 + k * pitch));
   rvp9_lf_transpose8x8(rows, cols);
   rvp9_lf_mask_hev_sse2(li, bl, th, cols[0], cols[1], cols[2], cols[3],
         cols[4], cols[5], cols[6], cols[7], &mask, &hev);
   rvp9_lf_filter4_sse2(mask, hev, &cols[2], &cols[3], &cols[4], &cols[5]);
   rvp9_lf_transpose8x8(cols, rows);
   for (k = 0; k < 8; k++)
      _mm_storel_epi64((__m128i*)(s - 4 + k * pitch), rows[k]);
}

static void rvp9_lpf_vertical_8_sse2(uint8_t *s, int pitch,
      uint8_t blimit, uint8_t limit, uint8_t thresh)
{
   __m128i bl = _mm_set1_epi8((char)blimit);
   __m128i li = _mm_set1_epi8((char)limit);
   __m128i th = _mm_set1_epi8((char)thresh);
   __m128i rows[8], cols[8], mask, hev, flat;
   int k;
   for (k = 0; k < 8; k++)
      rows[k] = _mm_loadl_epi64((const __m128i*)(s - 4 + k * pitch));
   rvp9_lf_transpose8x8(rows, cols);
   rvp9_lf_mask_hev_sse2(li, bl, th, cols[0], cols[1], cols[2], cols[3],
         cols[4], cols[5], cols[6], cols[7], &mask, &hev);
   flat = rvp9_lf_flat4_sse2(cols[0], cols[1], cols[2], cols[3],
         cols[4], cols[5], cols[6], cols[7]);
   rvp9_lf_filter8_sse2(mask, hev, flat, cols[0], &cols[1], &cols[2],
         &cols[3], &cols[4], &cols[5], &cols[6], cols[7]);
   rvp9_lf_transpose8x8(cols, rows);
   for (k = 0; k < 8; k++)
      _mm_storel_epi64((__m128i*)(s - 4 + k * pitch), rows[k]);
}

static void rvp9_lpf_vertical_16_sse2(uint8_t *s, int pitch,
      uint8_t blimit, uint8_t limit, uint8_t thresh)
{
   __m128i bl = _mm_set1_epi8((char)blimit);
   __m128i li = _mm_set1_epi8((char)limit);
   __m128i th = _mm_set1_epi8((char)thresh);
   __m128i lo[8], hi[8], r[16], mask, hev, flat, flat2, m5;
   int k;
   for (k = 0; k < 8; k++)
   {
      __m128i row = _mm_loadu_si128((const __m128i*)(s - 8 + k * pitch));
      lo[k] = row;
      hi[k] = _mm_srli_si128(row, 8);
   }
   rvp9_lf_transpose8x8(lo, r);        /* p7..p0 into r[0..7]  */
   rvp9_lf_transpose8x8(hi, r + 8);    /* q0..q7 into r[8..15] */
   rvp9_lf_mask_hev_sse2(li, bl, th, r[4], r[5], r[6], r[7],
         r[8], r[9], r[10], r[11], &mask, &hev);
   flat = rvp9_lf_flat4_sse2(r[4], r[5], r[6], r[7],
         r[8], r[9], r[10], r[11]);
   m5 = _mm_max_epu8(rvp9_lf_absd8(r[0], r[7]), rvp9_lf_absd8(r[15], r[8]));
   m5 = _mm_max_epu8(m5, rvp9_lf_absd8(r[1], r[7]));
   m5 = _mm_max_epu8(m5, rvp9_lf_absd8(r[14], r[8]));
   m5 = _mm_max_epu8(m5, rvp9_lf_absd8(r[2], r[7]));
   m5 = _mm_max_epu8(m5, rvp9_lf_absd8(r[13], r[8]));
   m5 = _mm_max_epu8(m5, rvp9_lf_absd8(r[3], r[7]));
   m5 = _mm_max_epu8(m5, rvp9_lf_absd8(r[12], r[8]));
   flat2 = rvp9_lf_le8(m5, _mm_set1_epi8(1));
   rvp9_lf_filter16_sse2(mask, hev, flat, flat2, r);
   rvp9_lf_transpose8x8(r, lo);
   rvp9_lf_transpose8x8(r + 8, hi);
   for (k = 0; k < 8; k++)
   {
      _mm_storel_epi64((__m128i*)(s - 8 + k * pitch), lo[k]);
      _mm_storel_epi64((__m128i*)(s     + k * pitch), hi[k]);
   }
}
#endif /* RVP9_SSE2 */

#if defined(RVP9_NEON) && !RVP9_RECON_HBD
/* NEON translation of the SSE2 loop filter above; the same integer
 * expressions lane for lane (vabd/vmax/vcle for the masks, vqadd/vqsub
 * signed-saturating int8 for filter4 with a native arithmetic
 * vshr_n_s8, u16 sliding sums with vrshrq rounding for the smoothing
 * taps, vbsl for the cascade blends). */

static void rvp9_lf_mask_hev_neon(uint8x8_t limit, uint8x8_t blimit,
      uint8x8_t thresh,
      uint8x8_t p3, uint8x8_t p2, uint8x8_t p1, uint8x8_t p0,
      uint8x8_t q0, uint8x8_t q1, uint8x8_t q2, uint8x8_t q3,
      uint8x8_t *mask, uint8x8_t *hev)
{
   uint8x8_t m = vmax_u8(vabd_u8(p3, p2), vabd_u8(p2, p1));
   uint16x8_t t;
   uint8x8_t in_blimit;
   m = vmax_u8(m, vabd_u8(p1, p0));
   m = vmax_u8(m, vabd_u8(q1, q0));
   m = vmax_u8(m, vabd_u8(q2, q1));
   m = vmax_u8(m, vabd_u8(q3, q2));

   /* abs(p0-q0)*2 + abs(p1-q1)/2 <= blimit, exact in 16-bit */
   t = vaddq_u16(vshlq_n_u16(vmovl_u8(vabd_u8(p0, q0)), 1),
                 vshrq_n_u16(vmovl_u8(vabd_u8(p1, q1)), 1));
   in_blimit = vmovn_u16(vcleq_u16(t, vmovl_u8(blimit)));

   *mask = vand_u8(vcle_u8(m, limit), in_blimit);
   *hev  = vcgt_u8(vmax_u8(vabd_u8(p1, p0), vabd_u8(q1, q0)), thresh);
}

static INLINE uint8x8_t rvp9_lf_flat4_neon(uint8x8_t p3, uint8x8_t p2,
      uint8x8_t p1, uint8x8_t p0, uint8x8_t q0, uint8x8_t q1,
      uint8x8_t q2, uint8x8_t q3)
{
   uint8x8_t m = vmax_u8(vabd_u8(p1, p0), vabd_u8(q1, q0));
   m = vmax_u8(m, vabd_u8(p2, p0));
   m = vmax_u8(m, vabd_u8(q2, q0));
   m = vmax_u8(m, vabd_u8(p3, p0));
   m = vmax_u8(m, vabd_u8(q3, q0));
   return vcle_u8(m, vdup_n_u8(1));
}

static void rvp9_lf_filter4_neon(uint8x8_t mask, uint8x8_t hev,
      uint8x8_t *p1, uint8x8_t *p0, uint8x8_t *q0, uint8x8_t *q1)
{
   const int8x8_t sb = vdup_n_s8((int8_t)0x80);
   int8x8_t ps1 = veor_s8(vreinterpret_s8_u8(*p1), sb);
   int8x8_t ps0 = veor_s8(vreinterpret_s8_u8(*p0), sb);
   int8x8_t qs0 = veor_s8(vreinterpret_s8_u8(*q0), sb);
   int8x8_t qs1 = veor_s8(vreinterpret_s8_u8(*q1), sb);
   int8x8_t filt, work, f1, f2, f3;

   filt = vand_s8(vqsub_s8(ps1, qs1), vreinterpret_s8_u8(hev));
   work = vqsub_s8(qs0, ps0);
   filt = vqadd_s8(filt, work);
   filt = vqadd_s8(filt, work);
   filt = vqadd_s8(filt, work);
   filt = vand_s8(filt, vreinterpret_s8_u8(mask));

   f1 = vshr_n_s8(vqadd_s8(filt, vdup_n_s8(4)), 3);
   f2 = vshr_n_s8(vqadd_s8(filt, vdup_n_s8(3)), 3);

   *q0 = vreinterpret_u8_s8(veor_s8(vqsub_s8(qs0, f1), sb));
   *p0 = vreinterpret_u8_s8(veor_s8(vqadd_s8(ps0, f2), sb));

   f3 = vshr_n_s8(vqadd_s8(f1, vdup_n_s8(1)), 1);
   f3 = vbic_s8(f3, vreinterpret_s8_u8(hev));

   *q1 = vreinterpret_u8_s8(veor_s8(vqsub_s8(qs1, f3), sb));
   *p1 = vreinterpret_u8_s8(veor_s8(vqadd_s8(ps1, f3), sb));
}

static INLINE uint8x8_t rvp9_lf_r3_neon(uint16x8_t sum)
{
   return vmovn_u16(vshrq_n_u16(vaddq_u16(sum, vdupq_n_u16(4)), 3));
}

static INLINE uint8x8_t rvp9_lf_r4_neon(uint16x8_t sum)
{
   return vmovn_u16(vshrq_n_u16(vaddq_u16(sum, vdupq_n_u16(8)), 4));
}

static void rvp9_lf_filter8_neon(uint8x8_t mask, uint8x8_t hev,
      uint8x8_t flat, uint8x8_t p3, uint8x8_t *p2, uint8x8_t *p1,
      uint8x8_t *p0, uint8x8_t *q0, uint8x8_t *q1, uint8x8_t *q2,
      uint8x8_t q3)
{
   uint8x8_t sel = vand_u8(flat, mask);
   uint8x8_t f_p1 = *p1, f_p0 = *p0, f_q0 = *q0, f_q1 = *q1;
   uint16x8_t P3 = vmovl_u8(p3),  P2 = vmovl_u8(*p2);
   uint16x8_t P1 = vmovl_u8(*p1), P0 = vmovl_u8(*p0);
   uint16x8_t Q0 = vmovl_u8(*q0), Q1 = vmovl_u8(*q1);
   uint16x8_t Q2 = vmovl_u8(*q2), Q3 = vmovl_u8(q3);
   uint16x8_t sum;
   uint8x8_t s_p2, s_p1, s_p0, s_q0, s_q1, s_q2;

   rvp9_lf_filter4_neon(mask, hev, &f_p1, &f_p0, &f_q0, &f_q1);

   sum  = vaddq_u16(vaddq_u16(P3, P3),
          vaddq_u16(vaddq_u16(P3, P2),
          vaddq_u16(vaddq_u16(P2, P1), vaddq_u16(P0, Q0))));
   s_p2 = rvp9_lf_r3_neon(sum);
   sum  = vsubq_u16(vaddq_u16(sum, vaddq_u16(P1, Q1)), vaddq_u16(P3, P2));
   s_p1 = rvp9_lf_r3_neon(sum);
   sum  = vsubq_u16(vaddq_u16(sum, vaddq_u16(P0, Q2)), vaddq_u16(P3, P1));
   s_p0 = rvp9_lf_r3_neon(sum);
   sum  = vsubq_u16(vaddq_u16(sum, vaddq_u16(Q0, Q3)), vaddq_u16(P3, P0));
   s_q0 = rvp9_lf_r3_neon(sum);
   sum  = vsubq_u16(vaddq_u16(sum, vaddq_u16(Q1, Q3)), vaddq_u16(P2, Q0));
   s_q1 = rvp9_lf_r3_neon(sum);
   sum  = vsubq_u16(vaddq_u16(sum, vaddq_u16(Q2, Q3)), vaddq_u16(P1, Q1));
   s_q2 = rvp9_lf_r3_neon(sum);

   *p2 = vbsl_u8(sel, s_p2, *p2);
   *p1 = vbsl_u8(sel, s_p1, f_p1);
   *p0 = vbsl_u8(sel, s_p0, f_p0);
   *q0 = vbsl_u8(sel, s_q0, f_q0);
   *q1 = vbsl_u8(sel, s_q1, f_q1);
   *q2 = vbsl_u8(sel, s_q2, *q2);
}

static void rvp9_lf_filter16_neon(uint8x8_t mask, uint8x8_t hev,
      uint8x8_t flat, uint8x8_t flat2, uint8x8_t r[16])
{
   uint8x8_t sel = vand_u8(flat2, vand_u8(flat, mask));
   uint16x8_t W[16], sum;
   uint8x8_t out[14];
   int i;

   for (i = 0; i < 16; i++)
      W[i] = vmovl_u8(r[i]);

   rvp9_lf_filter8_neon(mask, hev, flat, r[4], &r[5], &r[6], &r[7],
         &r[8], &r[9], &r[10], r[11]);

   sum = vaddq_u16(vshlq_n_u16(W[0], 3), W[1]);
   sum = vsubq_u16(sum, W[0]);
   sum = vaddq_u16(sum, W[1]);
   for (i = 2; i <= 8; i++)
      sum = vaddq_u16(sum, W[i]);
   out[0] = rvp9_lf_r4_neon(sum);
   sum = vsubq_u16(vaddq_u16(sum, vaddq_u16(W[2], W[9])),
         vaddq_u16(W[0], W[1]));
   out[1] = rvp9_lf_r4_neon(sum);
   sum = vsubq_u16(vaddq_u16(sum, vaddq_u16(W[3], W[10])),
         vaddq_u16(W[0], W[2]));
   out[2] = rvp9_lf_r4_neon(sum);
   sum = vsubq_u16(vaddq_u16(sum, vaddq_u16(W[4], W[11])),
         vaddq_u16(W[0], W[3]));
   out[3] = rvp9_lf_r4_neon(sum);
   sum = vsubq_u16(vaddq_u16(sum, vaddq_u16(W[5], W[12])),
         vaddq_u16(W[0], W[4]));
   out[4] = rvp9_lf_r4_neon(sum);
   sum = vsubq_u16(vaddq_u16(sum, vaddq_u16(W[6], W[13])),
         vaddq_u16(W[0], W[5]));
   out[5] = rvp9_lf_r4_neon(sum);
   sum = vsubq_u16(vaddq_u16(sum, vaddq_u16(W[7], W[14])),
         vaddq_u16(W[0], W[6]));
   out[6] = rvp9_lf_r4_neon(sum);
   sum = vsubq_u16(vaddq_u16(sum, vaddq_u16(W[8], W[15])),
         vaddq_u16(W[0], W[7]));
   out[7] = rvp9_lf_r4_neon(sum);
   sum = vsubq_u16(vaddq_u16(sum, vaddq_u16(W[9],  W[15])),
         vaddq_u16(W[1], W[8]));
   out[8] = rvp9_lf_r4_neon(sum);
   sum = vsubq_u16(vaddq_u16(sum, vaddq_u16(W[10], W[15])),
         vaddq_u16(W[2], W[9]));
   out[9] = rvp9_lf_r4_neon(sum);
   sum = vsubq_u16(vaddq_u16(sum, vaddq_u16(W[11], W[15])),
         vaddq_u16(W[3], W[10]));
   out[10] = rvp9_lf_r4_neon(sum);
   sum = vsubq_u16(vaddq_u16(sum, vaddq_u16(W[12], W[15])),
         vaddq_u16(W[4], W[11]));
   out[11] = rvp9_lf_r4_neon(sum);
   sum = vsubq_u16(vaddq_u16(sum, vaddq_u16(W[13], W[15])),
         vaddq_u16(W[5], W[12]));
   out[12] = rvp9_lf_r4_neon(sum);
   sum = vsubq_u16(vaddq_u16(sum, vaddq_u16(W[14], W[15])),
         vaddq_u16(W[6], W[13]));
   out[13] = rvp9_lf_r4_neon(sum);

   for (i = 0; i < 14; i++)
      r[i + 1] = vbsl_u8(sel, out[i], r[i + 1]);
}

/* 8x8 byte transpose via vtrn cascades */
static void rvp9_lf_transpose8x8_neon(const uint8x8_t *in, uint8x8_t *out)
{
   uint8x8x2_t  b0 = vtrn_u8(in[0], in[1]);
   uint8x8x2_t  b1 = vtrn_u8(in[2], in[3]);
   uint8x8x2_t  b2 = vtrn_u8(in[4], in[5]);
   uint8x8x2_t  b3 = vtrn_u8(in[6], in[7]);
   uint16x4x2_t c0 = vtrn_u16(vreinterpret_u16_u8(b0.val[0]),
                              vreinterpret_u16_u8(b1.val[0]));
   uint16x4x2_t c1 = vtrn_u16(vreinterpret_u16_u8(b0.val[1]),
                              vreinterpret_u16_u8(b1.val[1]));
   uint16x4x2_t c2 = vtrn_u16(vreinterpret_u16_u8(b2.val[0]),
                              vreinterpret_u16_u8(b3.val[0]));
   uint16x4x2_t c3 = vtrn_u16(vreinterpret_u16_u8(b2.val[1]),
                              vreinterpret_u16_u8(b3.val[1]));
   uint32x2x2_t d0 = vtrn_u32(vreinterpret_u32_u16(c0.val[0]),
                              vreinterpret_u32_u16(c2.val[0]));
   uint32x2x2_t d1 = vtrn_u32(vreinterpret_u32_u16(c1.val[0]),
                              vreinterpret_u32_u16(c3.val[0]));
   uint32x2x2_t d2 = vtrn_u32(vreinterpret_u32_u16(c0.val[1]),
                              vreinterpret_u32_u16(c2.val[1]));
   uint32x2x2_t d3 = vtrn_u32(vreinterpret_u32_u16(c1.val[1]),
                              vreinterpret_u32_u16(c3.val[1]));
   out[0] = vreinterpret_u8_u32(d0.val[0]);
   out[1] = vreinterpret_u8_u32(d1.val[0]);
   out[2] = vreinterpret_u8_u32(d2.val[0]);
   out[3] = vreinterpret_u8_u32(d3.val[0]);
   out[4] = vreinterpret_u8_u32(d0.val[1]);
   out[5] = vreinterpret_u8_u32(d1.val[1]);
   out[6] = vreinterpret_u8_u32(d2.val[1]);
   out[7] = vreinterpret_u8_u32(d3.val[1]);
}

static void rvp9_lpf_horizontal_4_neon(uint8_t *s, int pitch,
      uint8_t blimit, uint8_t limit, uint8_t thresh)
{
   uint8x8_t bl = vdup_n_u8(blimit), li = vdup_n_u8(limit);
   uint8x8_t th = vdup_n_u8(thresh);
   uint8x8_t p3 = vld1_u8(s - 4 * pitch), p2 = vld1_u8(s - 3 * pitch);
   uint8x8_t p1 = vld1_u8(s - 2 * pitch), p0 = vld1_u8(s - 1 * pitch);
   uint8x8_t q0 = vld1_u8(s),             q1 = vld1_u8(s + 1 * pitch);
   uint8x8_t q2 = vld1_u8(s + 2 * pitch), q3 = vld1_u8(s + 3 * pitch);
   uint8x8_t mask, hev;
   rvp9_lf_mask_hev_neon(li, bl, th, p3, p2, p1, p0, q0, q1, q2, q3,
         &mask, &hev);
   rvp9_lf_filter4_neon(mask, hev, &p1, &p0, &q0, &q1);
   vst1_u8(s - 2 * pitch, p1);
   vst1_u8(s - 1 * pitch, p0);
   vst1_u8(s,             q0);
   vst1_u8(s + 1 * pitch, q1);
}

static void rvp9_lpf_horizontal_8_neon(uint8_t *s, int pitch,
      uint8_t blimit, uint8_t limit, uint8_t thresh)
{
   uint8x8_t bl = vdup_n_u8(blimit), li = vdup_n_u8(limit);
   uint8x8_t th = vdup_n_u8(thresh);
   uint8x8_t p3 = vld1_u8(s - 4 * pitch), p2 = vld1_u8(s - 3 * pitch);
   uint8x8_t p1 = vld1_u8(s - 2 * pitch), p0 = vld1_u8(s - 1 * pitch);
   uint8x8_t q0 = vld1_u8(s),             q1 = vld1_u8(s + 1 * pitch);
   uint8x8_t q2 = vld1_u8(s + 2 * pitch), q3 = vld1_u8(s + 3 * pitch);
   uint8x8_t mask, hev, flat;
   rvp9_lf_mask_hev_neon(li, bl, th, p3, p2, p1, p0, q0, q1, q2, q3,
         &mask, &hev);
   flat = rvp9_lf_flat4_neon(p3, p2, p1, p0, q0, q1, q2, q3);
   rvp9_lf_filter8_neon(mask, hev, flat, p3, &p2, &p1, &p0,
         &q0, &q1, &q2, q3);
   vst1_u8(s - 3 * pitch, p2);
   vst1_u8(s - 2 * pitch, p1);
   vst1_u8(s - 1 * pitch, p0);
   vst1_u8(s,             q0);
   vst1_u8(s + 1 * pitch, q1);
   vst1_u8(s + 2 * pitch, q2);
}

static void rvp9_lpf_horizontal_16_neon(uint8_t *s, int pitch,
      uint8_t blimit, uint8_t limit, uint8_t thresh)
{
   uint8x8_t bl = vdup_n_u8(blimit), li = vdup_n_u8(limit);
   uint8x8_t th = vdup_n_u8(thresh);
   uint8x8_t r[16], mask, hev, flat, flat2, m5;
   int k;
   for (k = 0; k < 16; k++)
      r[k] = vld1_u8(s + (k - 8) * pitch);
   rvp9_lf_mask_hev_neon(li, bl, th, r[4], r[5], r[6], r[7],
         r[8], r[9], r[10], r[11], &mask, &hev);
   flat = rvp9_lf_flat4_neon(r[4], r[5], r[6], r[7],
         r[8], r[9], r[10], r[11]);
   m5 = vmax_u8(vabd_u8(r[0], r[7]), vabd_u8(r[15], r[8]));
   m5 = vmax_u8(m5, vabd_u8(r[1], r[7]));
   m5 = vmax_u8(m5, vabd_u8(r[14], r[8]));
   m5 = vmax_u8(m5, vabd_u8(r[2], r[7]));
   m5 = vmax_u8(m5, vabd_u8(r[13], r[8]));
   m5 = vmax_u8(m5, vabd_u8(r[3], r[7]));
   m5 = vmax_u8(m5, vabd_u8(r[12], r[8]));
   flat2 = vcle_u8(m5, vdup_n_u8(1));
   rvp9_lf_filter16_neon(mask, hev, flat, flat2, r);
   for (k = 1; k < 15; k++)
      vst1_u8(s + (k - 8) * pitch, r[k]);
}

static void rvp9_lpf_vertical_4_neon(uint8_t *s, int pitch,
      uint8_t blimit, uint8_t limit, uint8_t thresh)
{
   uint8x8_t bl = vdup_n_u8(blimit), li = vdup_n_u8(limit);
   uint8x8_t th = vdup_n_u8(thresh);
   uint8x8_t rows[8], cols[8], mask, hev;
   int k;
   for (k = 0; k < 8; k++)
      rows[k] = vld1_u8(s - 4 + k * pitch);
   rvp9_lf_transpose8x8_neon(rows, cols);
   rvp9_lf_mask_hev_neon(li, bl, th, cols[0], cols[1], cols[2], cols[3],
         cols[4], cols[5], cols[6], cols[7], &mask, &hev);
   rvp9_lf_filter4_neon(mask, hev, &cols[2], &cols[3], &cols[4], &cols[5]);
   rvp9_lf_transpose8x8_neon(cols, rows);
   for (k = 0; k < 8; k++)
      vst1_u8(s - 4 + k * pitch, rows[k]);
}

static void rvp9_lpf_vertical_8_neon(uint8_t *s, int pitch,
      uint8_t blimit, uint8_t limit, uint8_t thresh)
{
   uint8x8_t bl = vdup_n_u8(blimit), li = vdup_n_u8(limit);
   uint8x8_t th = vdup_n_u8(thresh);
   uint8x8_t rows[8], cols[8], mask, hev, flat;
   int k;
   for (k = 0; k < 8; k++)
      rows[k] = vld1_u8(s - 4 + k * pitch);
   rvp9_lf_transpose8x8_neon(rows, cols);
   rvp9_lf_mask_hev_neon(li, bl, th, cols[0], cols[1], cols[2], cols[3],
         cols[4], cols[5], cols[6], cols[7], &mask, &hev);
   flat = rvp9_lf_flat4_neon(cols[0], cols[1], cols[2], cols[3],
         cols[4], cols[5], cols[6], cols[7]);
   rvp9_lf_filter8_neon(mask, hev, flat, cols[0], &cols[1], &cols[2],
         &cols[3], &cols[4], &cols[5], &cols[6], cols[7]);
   rvp9_lf_transpose8x8_neon(cols, rows);
   for (k = 0; k < 8; k++)
      vst1_u8(s - 4 + k * pitch, rows[k]);
}

static void rvp9_lpf_vertical_16_neon(uint8_t *s, int pitch,
      uint8_t blimit, uint8_t limit, uint8_t thresh)
{
   uint8x8_t bl = vdup_n_u8(blimit), li = vdup_n_u8(limit);
   uint8x8_t th = vdup_n_u8(thresh);
   uint8x8_t lo[8], hi[8], r[16], mask, hev, flat, flat2, m5;
   int k;
   for (k = 0; k < 8; k++)
   {
      lo[k] = vld1_u8(s - 8 + k * pitch);
      hi[k] = vld1_u8(s     + k * pitch);
   }
   rvp9_lf_transpose8x8_neon(lo, r);
   rvp9_lf_transpose8x8_neon(hi, r + 8);
   rvp9_lf_mask_hev_neon(li, bl, th, r[4], r[5], r[6], r[7],
         r[8], r[9], r[10], r[11], &mask, &hev);
   flat = rvp9_lf_flat4_neon(r[4], r[5], r[6], r[7],
         r[8], r[9], r[10], r[11]);
   m5 = vmax_u8(vabd_u8(r[0], r[7]), vabd_u8(r[15], r[8]));
   m5 = vmax_u8(m5, vabd_u8(r[1], r[7]));
   m5 = vmax_u8(m5, vabd_u8(r[14], r[8]));
   m5 = vmax_u8(m5, vabd_u8(r[2], r[7]));
   m5 = vmax_u8(m5, vabd_u8(r[13], r[8]));
   m5 = vmax_u8(m5, vabd_u8(r[3], r[7]));
   m5 = vmax_u8(m5, vabd_u8(r[12], r[8]));
   flat2 = vcle_u8(m5, vdup_n_u8(1));
   rvp9_lf_filter16_neon(mask, hev, flat, flat2, r);
   rvp9_lf_transpose8x8_neon(r, lo);
   rvp9_lf_transpose8x8_neon(r + 8, hi);
   for (k = 0; k < 8; k++)
   {
      vst1_u8(s - 8 + k * pitch, lo[k]);
      vst1_u8(s     + k * pitch, hi[k]);
   }
}
#endif /* RVP9_NEON */

static void rvp9_lpf_horizontal_4(RVP9_PIXEL *s, int pitch, const uint8_t *blimit,
                            const uint8_t *limit, const uint8_t *thresh) {
  int i;

#if defined(RVP9_SSE2) && !RVP9_RECON_HBD
  rvp9_lpf_horizontal_4_sse2(s, pitch, *blimit, *limit, *thresh);
  (void)i;
  return;
#elif defined(RVP9_NEON) && !RVP9_RECON_HBD
  rvp9_lpf_horizontal_4_neon(s, pitch, *blimit, *limit, *thresh);
  (void)i;
  return;
#endif

/* loop filter designed to work using chars so that we can make maximum use */
/* of 8 bit simd instructions. */
  for (i = 0; i < 8; ++i) {
    const RVP9_PIXEL p3 = s[-4 * pitch], p2 = s[-3 * pitch], p1 = s[-2 * pitch],
                  p0 = s[-pitch];
    const RVP9_PIXEL q0 = s[0 * pitch], q1 = s[1 * pitch], q2 = s[2 * pitch],
                  q3 = s[3 * pitch];
    const int8_t mask =
        rvp9_filter_mask(*limit, *blimit, p3, p2, p1, p0, q0, q1, q2, q3);
    rvp9_filter4(mask, *thresh, s - 2 * pitch, s - 1 * pitch, s, s + 1 * pitch);
    ++s;
  }
}

static void rvp9_lpf_vertical_4(RVP9_PIXEL *s, int pitch, const uint8_t *blimit,
                          const uint8_t *limit, const uint8_t *thresh) {
  int i;

#if defined(RVP9_SSE2) && !RVP9_RECON_HBD
  rvp9_lpf_vertical_4_sse2(s, pitch, *blimit, *limit, *thresh);
  (void)i;
  return;
#elif defined(RVP9_NEON) && !RVP9_RECON_HBD
  rvp9_lpf_vertical_4_neon(s, pitch, *blimit, *limit, *thresh);
  (void)i;
  return;
#endif

/* loop filter designed to work using chars so that we can make maximum use */
/* of 8 bit simd instructions. */
  for (i = 0; i < 8; ++i) {
    const RVP9_PIXEL p3 = s[-4], p2 = s[-3], p1 = s[-2], p0 = s[-1];
    const RVP9_PIXEL q0 = s[0], q1 = s[1], q2 = s[2], q3 = s[3];
    const int8_t mask =
        rvp9_filter_mask(*limit, *blimit, p3, p2, p1, p0, q0, q1, q2, q3);
    rvp9_filter4(mask, *thresh, s - 2, s - 1, s, s + 1);
    s += pitch;
  }
}

static void rvp9_lpf_horizontal_8(RVP9_PIXEL *s, int pitch, const uint8_t *blimit,
                            const uint8_t *limit, const uint8_t *thresh) {
  int i;

#if defined(RVP9_SSE2) && !RVP9_RECON_HBD
  rvp9_lpf_horizontal_8_sse2(s, pitch, *blimit, *limit, *thresh);
  (void)i;
  return;
#elif defined(RVP9_NEON) && !RVP9_RECON_HBD
  rvp9_lpf_horizontal_8_neon(s, pitch, *blimit, *limit, *thresh);
  (void)i;
  return;
#endif

/* loop filter designed to work using chars so that we can make maximum use */
/* of 8 bit simd instructions. */
  for (i = 0; i < 8; ++i) {
    const RVP9_PIXEL p3 = s[-4 * pitch], p2 = s[-3 * pitch], p1 = s[-2 * pitch],
                  p0 = s[-pitch];
    const RVP9_PIXEL q0 = s[0 * pitch], q1 = s[1 * pitch], q2 = s[2 * pitch],
                  q3 = s[3 * pitch];

    const int8_t mask =
        rvp9_filter_mask(*limit, *blimit, p3, p2, p1, p0, q0, q1, q2, q3);
    const int8_t flat = rvp9_flat_mask4(1, p3, p2, p1, p0, q0, q1, q2, q3);
    rvp9_filter8(mask, *thresh, flat, s - 4 * pitch, s - 3 * pitch, s - 2 * pitch,
            s - 1 * pitch, s, s + 1 * pitch, s + 2 * pitch, s + 3 * pitch);
    ++s;
  }
}

static void rvp9_lpf_vertical_8(RVP9_PIXEL *s, int pitch, const uint8_t *blimit,
                          const uint8_t *limit, const uint8_t *thresh) {
  int i;

#if defined(RVP9_SSE2) && !RVP9_RECON_HBD
  rvp9_lpf_vertical_8_sse2(s, pitch, *blimit, *limit, *thresh);
  (void)i;
  return;
#elif defined(RVP9_NEON) && !RVP9_RECON_HBD
  rvp9_lpf_vertical_8_neon(s, pitch, *blimit, *limit, *thresh);
  (void)i;
  return;
#endif

  for (i = 0; i < 8; ++i) {
    const RVP9_PIXEL p3 = s[-4], p2 = s[-3], p1 = s[-2], p0 = s[-1];
    const RVP9_PIXEL q0 = s[0], q1 = s[1], q2 = s[2], q3 = s[3];
    const int8_t mask =
        rvp9_filter_mask(*limit, *blimit, p3, p2, p1, p0, q0, q1, q2, q3);
    const int8_t flat = rvp9_flat_mask4(1, p3, p2, p1, p0, q0, q1, q2, q3);
    rvp9_filter8(mask, *thresh, flat, s - 4, s - 3, s - 2, s - 1, s, s + 1, s + 2,
            s + 3);
    s += pitch;
  }
}

static void rvp9_mb_lpf_horizontal_edge_w(RVP9_PIXEL *s, int pitch,
                                     const uint8_t *blimit,
                                     const uint8_t *limit,
                                     const uint8_t *thresh, int count) {
  int i;

#if defined(RVP9_SSE2) && !RVP9_RECON_HBD
  for (i = 0; i < count; i++)
     rvp9_lpf_horizontal_16_sse2(s + 8 * i, pitch, *blimit, *limit, *thresh);
  return;
#elif defined(RVP9_NEON) && !RVP9_RECON_HBD
  for (i = 0; i < count; i++)
     rvp9_lpf_horizontal_16_neon(s + 8 * i, pitch, *blimit, *limit, *thresh);
  return;
#endif

/* loop filter designed to work using chars so that we can make maximum use */
/* of 8 bit simd instructions. */
  for (i = 0; i < 8 * count; ++i) {
    const RVP9_PIXEL p3 = s[-4 * pitch], p2 = s[-3 * pitch], p1 = s[-2 * pitch],
                  p0 = s[-pitch];
    const RVP9_PIXEL q0 = s[0 * pitch], q1 = s[1 * pitch], q2 = s[2 * pitch],
                  q3 = s[3 * pitch];
    const int8_t mask =
        rvp9_filter_mask(*limit, *blimit, p3, p2, p1, p0, q0, q1, q2, q3);
    const int8_t flat = rvp9_flat_mask4(1, p3, p2, p1, p0, q0, q1, q2, q3);
    const int8_t flat2 = rvp9_flat_mask5(
        1, s[-8 * pitch], s[-7 * pitch], s[-6 * pitch], s[-5 * pitch], p0, q0,
        s[4 * pitch], s[5 * pitch], s[6 * pitch], s[7 * pitch]);

    rvp9_filter16(mask, *thresh, flat, flat2, s - 8 * pitch, s - 7 * pitch,
             s - 6 * pitch, s - 5 * pitch, s - 4 * pitch, s - 3 * pitch,
             s - 2 * pitch, s - 1 * pitch, s, s + 1 * pitch, s + 2 * pitch,
             s + 3 * pitch, s + 4 * pitch, s + 5 * pitch, s + 6 * pitch,
             s + 7 * pitch);
    ++s;
  }
}

static void rvp9_lpf_horizontal_16(RVP9_PIXEL *s, int pitch, const uint8_t *blimit,
                             const uint8_t *limit, const uint8_t *thresh) {
  rvp9_mb_lpf_horizontal_edge_w(s, pitch, blimit, limit, thresh, 1);
}

static void rvp9_mb_lpf_vertical_edge_w(RVP9_PIXEL *s, int pitch, const uint8_t *blimit,
                                   const uint8_t *limit, const uint8_t *thresh,
                                   int count) {
  int i;

#if defined(RVP9_SSE2) && !RVP9_RECON_HBD
  for (i = 0; i < count; i += 8)
     rvp9_lpf_vertical_16_sse2(s + i * pitch, pitch, *blimit, *limit, *thresh);
  return;
#elif defined(RVP9_NEON) && !RVP9_RECON_HBD
  for (i = 0; i < count; i += 8)
     rvp9_lpf_vertical_16_neon(s + i * pitch, pitch, *blimit, *limit, *thresh);
  return;
#endif

  for (i = 0; i < count; ++i) {
    const RVP9_PIXEL p3 = s[-4], p2 = s[-3], p1 = s[-2], p0 = s[-1];
    const RVP9_PIXEL q0 = s[0], q1 = s[1], q2 = s[2], q3 = s[3];
    const int8_t mask =
        rvp9_filter_mask(*limit, *blimit, p3, p2, p1, p0, q0, q1, q2, q3);
    const int8_t flat = rvp9_flat_mask4(1, p3, p2, p1, p0, q0, q1, q2, q3);
    const int8_t flat2 = rvp9_flat_mask5(1, s[-8], s[-7], s[-6], s[-5], p0, q0, s[4],
                                    s[5], s[6], s[7]);

    rvp9_filter16(mask, *thresh, flat, flat2, s - 8, s - 7, s - 6, s - 5, s - 4,
             s - 3, s - 2, s - 1, s, s + 1, s + 2, s + 3, s + 4, s + 5, s + 6,
             s + 7);
    s += pitch;
  }
}

static void rvp9_lpf_vertical_16(RVP9_PIXEL *s, int pitch, const uint8_t *blimit,
                           const uint8_t *limit, const uint8_t *thresh) {
  rvp9_mb_lpf_vertical_edge_w(s, pitch, blimit, limit, thresh, 8);
}

/* ==================================================================== */
/* Inter prediction: 8-tap convolve (vpx_dsp/vpx_convolve.c) + border   */
/* extension (build_mc_border) + per-block MC driver.                   */
/* ==================================================================== */
#define RVP9_SUBPEL_BITS  4
#define RVP9_SUBPEL_MASK  15
#define RVP9_SUBPEL_TAPS  8
#define RVP9_FILTER_BITS  7
#define RVP9_INTERP_EXTEND 4

#if RVP9_RECON_HBD && defined(RVP9_SSE2)
/* ---- 10/12-bit SSE2 8-tap convolve (unscaled, step_q4 == 16) ----
 * Width-major: 8 adjacent output columns per iteration.  For each of
 * the 8 taps a single contiguous 8-wide row load is multiplied by the
 * broadcast tap and accumulated (mullo/mulhi -> unpack to int32), so
 * there are no per-pixel gathers and no horizontal reductions - the
 * source of the ~2x win over the scalar loop.  +64, arithmetic >>7,
 * then clamp to [0, RVP9_PXMAX].  Columns past the last multiple of 8
 * fall to the scalar tail.  Bit-exact with the scalar path. */
static void rvp9_convolve_hbd_row8(const RVP9_PIXEL *s, int step,
      const int16_t *f, RVP9_PIXEL *dst, int avg)
{
   __m128i acc_lo = _mm_set1_epi32(64);
   __m128i acc_hi = _mm_set1_epi32(64);
   int k;
   for (k = 0; k < RVP9_SUBPEL_TAPS; k++)
   {
      __m128i row = _mm_loadu_si128((const __m128i*)(s + k * step));
      __m128i tap = _mm_set1_epi16(f[k]);
      __m128i lo  = _mm_mullo_epi16(row, tap);
      __m128i hi  = _mm_mulhi_epi16(row, tap);   /* signed: taps signed */
      acc_lo = _mm_add_epi32(acc_lo, _mm_unpacklo_epi16(lo, hi));
      acc_hi = _mm_add_epi32(acc_hi, _mm_unpackhi_epi16(lo, hi));
   }
   acc_lo = _mm_srai_epi32(acc_lo, 7);
   acc_hi = _mm_srai_epi32(acc_hi, 7);
   {
      uint32_t t[8]; int q;
      _mm_storeu_si128((__m128i*)t,     acc_lo);
      _mm_storeu_si128((__m128i*)(t+4), acc_hi);
      for (q = 0; q < 8; q++)
      {
         int r = (int)t[q];
         if (r < 0) r = 0; else if (r > RVP9_PXMAX) r = RVP9_PXMAX;
         if (avg) dst[q] = (RVP9_PIXEL)((dst[q] + r + 1) >> 1);
         else     dst[q] = (RVP9_PIXEL)r;
      }
   }
}

static void rvp9_convolve_horiz_hbd_sse2(const RVP9_PIXEL *src,
      int src_stride, RVP9_PIXEL *dst, int dst_stride,
      const int16_t *f, int w, int h, int avg)
{
   int x, y, k;
   for (y = 0; y < h; y++)
   {
      for (x = 0; x + 8 <= w; x += 8)
         rvp9_convolve_hbd_row8(src + x, 1, f, dst + x, avg);
      for (; x < w; x++)
      {
         const RVP9_PIXEL *s = src + x; int sum = 0;
         for (k = 0; k < RVP9_SUBPEL_TAPS; k++) sum += s[k] * f[k];
         { int r = (sum + 64) >> 7;
           if (r < 0) r = 0; else if (r > RVP9_PXMAX) r = RVP9_PXMAX;
           if (avg) dst[x] = (RVP9_PIXEL)((dst[x] + r + 1) >> 1);
           else     dst[x] = (RVP9_PIXEL)r; }
      }
      src += src_stride; dst += dst_stride;
   }
}

static void rvp9_convolve_vert_hbd_sse2(const RVP9_PIXEL *src,
      int src_stride, RVP9_PIXEL *dst, int dst_stride,
      const int16_t *f, int w, int h, int avg)
{
   int x, y, k;
   for (y = 0; y < h; y++)
   {
      const RVP9_PIXEL *s = src;
      for (x = 0; x + 8 <= w; x += 8)
         rvp9_convolve_hbd_row8(s + x, src_stride, f, dst + x, avg);
      for (; x < w; x++)
      {
         const RVP9_PIXEL *sc = s + x; int sum = 0;
         for (k = 0; k < RVP9_SUBPEL_TAPS; k++) sum += sc[k * src_stride] * f[k];
         { int r = (sum + 64) >> 7;
           if (r < 0) r = 0; else if (r > RVP9_PXMAX) r = RVP9_PXMAX;
           if (avg) dst[x] = (RVP9_PIXEL)((dst[x] + r + 1) >> 1);
           else     dst[x] = (RVP9_PIXEL)r; }
      }
      src += src_stride; dst += dst_stride;
   }
}
#endif /* RVP9_RECON_HBD && RVP9_SSE2 */


/* ---- SSE2 / NEON 8-tap convolve (unscaled path, step_q4 == 16) ----
 * With a 16/16th step the subpel phase is constant across the block, so
 * each call is a fixed 8-tap FIR. The SIMD paths compute the identical
 * integer expressions as the scalar loops: pmaddwd on (sample, sample)
 * x (tap, tap) pairs accumulating in int32, +64, arithmetic >>7, then
 * saturating packs for the clip; the averaging variant's
 * ROUND_POWER_OF_TWO(dst + res, 1) is exactly pavgb/vrhadd. Taps and
 * products fit int16 lanes (|tap| <= 128, samples <= 255). */
#if defined(RVP9_SSE2) && !RVP9_RECON_HBD
static void rvp9_convolve_horiz_sse2(const uint8_t *src, int src_stride,
      uint8_t *dst, int dst_stride, const int16_t *f, int w, int h, int avg)
{
   const __m128i zero = _mm_setzero_si128();
   const __m128i rnd  = _mm_set1_epi32(64);
   const __m128i f01  = _mm_set1_epi32(((uint32_t)(uint16_t)f[1] << 16) | (uint16_t)f[0]);
   const __m128i f23  = _mm_set1_epi32(((uint32_t)(uint16_t)f[3] << 16) | (uint16_t)f[2]);
   const __m128i f45  = _mm_set1_epi32(((uint32_t)(uint16_t)f[5] << 16) | (uint16_t)f[4]);
   const __m128i f67  = _mm_set1_epi32(((uint32_t)(uint16_t)f[7] << 16) | (uint16_t)f[6]);
   int x, y;

   for (y = 0; y < h; y++)
   {
      for (x = 0; x + 8 <= w; x += 8)
      {
         /* 8 outputs need samples s[x .. x+14] exactly; compose from
          * two 8-byte loads so not a single byte outside the scalar
          * footprint is touched (the source can be an exact-bounds
          * border temp). */
         __m128i lo = _mm_loadl_epi64((const __m128i*)(src + x));     /* s0..s7  */
         __m128i hi = _mm_loadl_epi64((const __m128i*)(src + x + 7)); /* s7..s14 */
         __m128i s0 = _mm_unpacklo_epi8(lo, zero);              /* s0..s7  */
         __m128i s8 = _mm_srli_si128(
               _mm_unpacklo_epi8(hi, zero), 2);                 /* s8..s14,0 */
         __m128i s1 = _mm_or_si128(_mm_srli_si128(s0, 2),
                                   _mm_slli_si128(s8, 14));     /* s1..s8  */
         __m128i s2 = _mm_or_si128(_mm_srli_si128(s0, 4),
                                   _mm_slli_si128(s8, 12));
         __m128i s3 = _mm_or_si128(_mm_srli_si128(s0, 6),
                                   _mm_slli_si128(s8, 10));
         __m128i s4 = _mm_or_si128(_mm_srli_si128(s0, 8),
                                   _mm_slli_si128(s8, 8));
         __m128i s5 = _mm_or_si128(_mm_srli_si128(s0, 10),
                                   _mm_slli_si128(s8, 6));
         __m128i s6 = _mm_or_si128(_mm_srli_si128(s0, 12),
                                   _mm_slli_si128(s8, 4));
         __m128i s7 = _mm_or_si128(_mm_srli_si128(s0, 14),
                                   _mm_slli_si128(s8, 2));
         __m128i a_lo, a_hi, r16, r8;
         a_lo = _mm_add_epi32(
               _mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(s0, s1), f01),
                             _mm_madd_epi16(_mm_unpacklo_epi16(s2, s3), f23)),
               _mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(s4, s5), f45),
                             _mm_madd_epi16(_mm_unpacklo_epi16(s6, s7), f67)));
         a_hi = _mm_add_epi32(
               _mm_add_epi32(_mm_madd_epi16(_mm_unpackhi_epi16(s0, s1), f01),
                             _mm_madd_epi16(_mm_unpackhi_epi16(s2, s3), f23)),
               _mm_add_epi32(_mm_madd_epi16(_mm_unpackhi_epi16(s4, s5), f45),
                             _mm_madd_epi16(_mm_unpackhi_epi16(s6, s7), f67)));
         a_lo = _mm_srai_epi32(_mm_add_epi32(a_lo, rnd), 7);
         a_hi = _mm_srai_epi32(_mm_add_epi32(a_hi, rnd), 7);
         r16  = _mm_packs_epi32(a_lo, a_hi);
         r8   = _mm_packus_epi16(r16, r16);
         if (avg)
            r8 = _mm_avg_epu8(r8,
                  _mm_loadl_epi64((const __m128i*)(dst + x)));
         _mm_storel_epi64((__m128i*)(dst + x), r8);
      }
      if (x + 4 <= w)
      {
         /* 4 outputs need s[x .. x+10] exactly: two 8-byte loads at
          * x and x+3. Only the low four lanes of each shifted vector
          * are consumed. */
         __m128i v0 = _mm_unpacklo_epi8(
               _mm_loadl_epi64((const __m128i*)(src + x)), zero);     /* s0..s7  */
         __m128i v3 = _mm_unpacklo_epi8(
               _mm_loadl_epi64((const __m128i*)(src + x + 3)), zero); /* s3..s10 */
         __m128i s1 = _mm_srli_si128(v0, 2);
         __m128i s2 = _mm_srli_si128(v0, 4);
         __m128i s3 = _mm_srli_si128(v0, 6);
         __m128i s4 = _mm_srli_si128(v0, 8);
         __m128i s5 = _mm_srli_si128(v3, 4);
         __m128i s6 = _mm_srli_si128(v3, 6);
         __m128i s7 = _mm_srli_si128(v3, 8);
         __m128i a_lo, r16, r8;
         a_lo = _mm_add_epi32(
               _mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(v0, s1), f01),
                             _mm_madd_epi16(_mm_unpacklo_epi16(s2, s3), f23)),
               _mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(s4, s5), f45),
                             _mm_madd_epi16(_mm_unpacklo_epi16(s6, s7), f67)));
         a_lo = _mm_srai_epi32(_mm_add_epi32(a_lo, rnd), 7);
         r16  = _mm_packs_epi32(a_lo, a_lo);
         r8   = _mm_packus_epi16(r16, r16);
         if (avg)
         {
            int32_t dv;
            memcpy(&dv, dst + x, 4);
            r8 = _mm_avg_epu8(r8, _mm_cvtsi32_si128(dv));
         }
         {
            int32_t ov = _mm_cvtsi128_si32(r8);
            memcpy(dst + x, &ov, 4);
         }
         x += 4;
      }
      for (; x < w; x++)
      {
         int k, sum = 0;
         for (k = 0; k < 8; k++)
            sum += src[x + k] * f[k];
         if (avg)
            dst[x] = (uint8_t)ROUND_POWER_OF_TWO(dst[x] +
                  rvp9_clip8(ROUND_POWER_OF_TWO(sum, 7)), 1);
         else
            dst[x] = rvp9_clip8(ROUND_POWER_OF_TWO(sum, 7));
      }
      src += src_stride;
      dst += dst_stride;
   }
}

static void rvp9_convolve_vert_sse2(const uint8_t *src, int src_stride,
      uint8_t *dst, int dst_stride, const int16_t *f, int w, int h, int avg)
{
   const __m128i zero = _mm_setzero_si128();
   const __m128i rnd  = _mm_set1_epi32(64);
   const __m128i f01  = _mm_set1_epi32(((uint32_t)(uint16_t)f[1] << 16) | (uint16_t)f[0]);
   const __m128i f23  = _mm_set1_epi32(((uint32_t)(uint16_t)f[3] << 16) | (uint16_t)f[2]);
   const __m128i f45  = _mm_set1_epi32(((uint32_t)(uint16_t)f[5] << 16) | (uint16_t)f[4]);
   const __m128i f67  = _mm_set1_epi32(((uint32_t)(uint16_t)f[7] << 16) | (uint16_t)f[6]);
   int x, y;

   for (x = 0; x + 8 <= w; x += 8)
   {
      const uint8_t *s = src + x;
      uint8_t *dp      = dst + x;
      for (y = 0; y < h; y++)
      {
         __m128i r0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*)(s + 0*src_stride)), zero);
         __m128i r1 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*)(s + 1*src_stride)), zero);
         __m128i r2 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*)(s + 2*src_stride)), zero);
         __m128i r3 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*)(s + 3*src_stride)), zero);
         __m128i r4 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*)(s + 4*src_stride)), zero);
         __m128i r5 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*)(s + 5*src_stride)), zero);
         __m128i r6 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*)(s + 6*src_stride)), zero);
         __m128i r7 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*)(s + 7*src_stride)), zero);
         __m128i a_lo, a_hi, r16, r8;
         a_lo = _mm_add_epi32(
               _mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(r0, r1), f01),
                             _mm_madd_epi16(_mm_unpacklo_epi16(r2, r3), f23)),
               _mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(r4, r5), f45),
                             _mm_madd_epi16(_mm_unpacklo_epi16(r6, r7), f67)));
         a_hi = _mm_add_epi32(
               _mm_add_epi32(_mm_madd_epi16(_mm_unpackhi_epi16(r0, r1), f01),
                             _mm_madd_epi16(_mm_unpackhi_epi16(r2, r3), f23)),
               _mm_add_epi32(_mm_madd_epi16(_mm_unpackhi_epi16(r4, r5), f45),
                             _mm_madd_epi16(_mm_unpackhi_epi16(r6, r7), f67)));
         a_lo = _mm_srai_epi32(_mm_add_epi32(a_lo, rnd), 7);
         a_hi = _mm_srai_epi32(_mm_add_epi32(a_hi, rnd), 7);
         r16  = _mm_packs_epi32(a_lo, a_hi);
         r8   = _mm_packus_epi16(r16, r16);
         if (avg)
            r8 = _mm_avg_epu8(r8, _mm_loadl_epi64((const __m128i*)dp));
         _mm_storel_epi64((__m128i*)dp, r8);
         s  += src_stride;
         dp += dst_stride;
      }
   }
   if (x + 4 <= w)
   {
      const uint8_t *s = src + x;
      uint8_t *dp      = dst + x;
      for (y = 0; y < h; y++)
      {
         __m128i r0, r1, r2, r3, r4, r5, r6, r7, a_lo, r16, r8;
         int32_t t;
#define RVP9_LD4(P) \
         (memcpy(&t, (P), 4), _mm_unpacklo_epi8(_mm_cvtsi32_si128(t), zero))
         r0 = RVP9_LD4(s + 0*src_stride);
         r1 = RVP9_LD4(s + 1*src_stride);
         r2 = RVP9_LD4(s + 2*src_stride);
         r3 = RVP9_LD4(s + 3*src_stride);
         r4 = RVP9_LD4(s + 4*src_stride);
         r5 = RVP9_LD4(s + 5*src_stride);
         r6 = RVP9_LD4(s + 6*src_stride);
         r7 = RVP9_LD4(s + 7*src_stride);
#undef RVP9_LD4
         a_lo = _mm_add_epi32(
               _mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(r0, r1), f01),
                             _mm_madd_epi16(_mm_unpacklo_epi16(r2, r3), f23)),
               _mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(r4, r5), f45),
                             _mm_madd_epi16(_mm_unpacklo_epi16(r6, r7), f67)));
         a_lo = _mm_srai_epi32(_mm_add_epi32(a_lo, rnd), 7);
         r16  = _mm_packs_epi32(a_lo, a_lo);
         r8   = _mm_packus_epi16(r16, r16);
         if (avg)
         {
            memcpy(&t, dp, 4);
            r8 = _mm_avg_epu8(r8, _mm_cvtsi32_si128(t));
         }
         t = _mm_cvtsi128_si32(r8);
         memcpy(dp, &t, 4);
         s  += src_stride;
         dp += dst_stride;
      }
      x += 4;
   }
   for (; x < w; x++)
   {
      const uint8_t *s = src + x;
      uint8_t *dp      = dst + x;
      for (y = 0; y < h; y++)
      {
         int k, sum = 0;
         for (k = 0; k < 8; k++)
            sum += s[k * src_stride] * f[k];
         if (avg)
            *dp = (uint8_t)ROUND_POWER_OF_TWO(*dp +
                  rvp9_clip8(ROUND_POWER_OF_TWO(sum, 7)), 1);
         else
            *dp = rvp9_clip8(ROUND_POWER_OF_TWO(sum, 7));
         s  += src_stride;
         dp += dst_stride;
      }
   }
}
#endif

#if defined(RVP9_NEON) && !RVP9_RECON_HBD
/* NEON translation of the SSE2 kernels: vmlal_lane tap chains
 * accumulating in int32; vqrshrn_n_s32(sum, 7) is exactly
 * (sum + 64) >> 7 with int16 saturation, vqmovun the 0..255 clip,
 * and vrhadd_u8 exactly ROUND_POWER_OF_TWO(a + b, 1). */
static void rvp9_convolve_horiz_neon(const uint8_t *src, int src_stride,
      uint8_t *dst, int dst_stride, const int16_t *f, int w, int h, int avg)
{
   const int16x4_t flo = vld1_s16(f);
   const int16x4_t fhi = vld1_s16(f + 4);
   int x, y;

   for (y = 0; y < h; y++)
   {
      for (x = 0; x + 8 <= w; x += 8)
      {
         /* Exact scalar footprint (s[x .. x+14]) via two 8-byte
          * loads; v7 holds s7..s14, t8 rotates it to s8..s14 in lanes
          * 0..6 (lane 7 is never consumed: vext(v0, t8, k) with k <= 7
          * reads t8 lanes 0..k-1 <= 6). */
         int16x8_t v0 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(src + x)));
         int16x8_t v7 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(src + x + 7)));
         int16x8_t t8 = vextq_s16(v7, v7, 1);
         int16x8_t s1, s2, s3, s4, s5, s6, s7;
         int32x4_t a_lo, a_hi;
         uint8x8_t r8;
         s1 = vextq_s16(v0, t8, 1);
         s2 = vextq_s16(v0, t8, 2);
         s3 = vextq_s16(v0, t8, 3);
         s4 = vextq_s16(v0, t8, 4);
         s5 = vextq_s16(v0, t8, 5);
         s6 = vextq_s16(v0, t8, 6);
         s7 = vextq_s16(v0, t8, 7);

         a_lo = vmull_lane_s16(vget_low_s16(v0), flo, 0);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(s1), flo, 1);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(s2), flo, 2);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(s3), flo, 3);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(s4), fhi, 0);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(s5), fhi, 1);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(s6), fhi, 2);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(s7), fhi, 3);
         a_hi = vmull_lane_s16(vget_high_s16(v0), flo, 0);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(s1), flo, 1);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(s2), flo, 2);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(s3), flo, 3);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(s4), fhi, 0);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(s5), fhi, 1);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(s6), fhi, 2);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(s7), fhi, 3);

         r8 = vqmovun_s16(vcombine_s16(vqrshrn_n_s32(a_lo, 7),
                                       vqrshrn_n_s32(a_hi, 7)));
         if (avg)
            r8 = vrhadd_u8(r8, vld1_u8(dst + x));
         vst1_u8(dst + x, r8);
      }
      if (x + 4 <= w)
      {
         /* 4 outputs need s[x .. x+10] exactly */
         uint8_t tmp[16];
         int16x8_t v0, v3, s1, s2, s3, s4, s5, s6, s7;
         int32x4_t a_lo;
         uint8x8_t r8;
         memcpy(tmp,     src + x,     8);
         memcpy(tmp + 8, src + x + 3, 8);
         v0 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(tmp)));
         v3 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(tmp + 8)));
         s1 = vextq_s16(v0, v0, 1);
         s2 = vextq_s16(v0, v0, 2);
         s3 = vextq_s16(v0, v0, 3);
         s4 = vextq_s16(v0, v0, 4);
         s5 = vextq_s16(v3, v3, 2);
         s6 = vextq_s16(v3, v3, 3);
         s7 = vextq_s16(v3, v3, 4);
         a_lo = vmull_lane_s16(vget_low_s16(v0), flo, 0);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(s1), flo, 1);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(s2), flo, 2);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(s3), flo, 3);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(s4), fhi, 0);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(s5), fhi, 1);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(s6), fhi, 2);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(s7), fhi, 3);
         {
            int16x4_t n16 = vqrshrn_n_s32(a_lo, 7);
            r8 = vqmovun_s16(vcombine_s16(n16, n16));
         }
         if (avg)
         {
            uint8_t dv[8];
            memcpy(dv, dst + x, 4);
            memset(dv + 4, 0, 4);
            r8 = vrhadd_u8(r8, vld1_u8(dv));
         }
         {
            uint8_t ov[8];
            vst1_u8(ov, r8);
            memcpy(dst + x, ov, 4);
         }
         x += 4;
      }
      for (; x < w; x++)
      {
         int k, sum = 0;
         for (k = 0; k < 8; k++)
            sum += src[x + k] * f[k];
         if (avg)
            dst[x] = (uint8_t)ROUND_POWER_OF_TWO(dst[x] +
                  rvp9_clip8(ROUND_POWER_OF_TWO(sum, 7)), 1);
         else
            dst[x] = rvp9_clip8(ROUND_POWER_OF_TWO(sum, 7));
      }
      src += src_stride;
      dst += dst_stride;
   }
}

static void rvp9_convolve_vert_neon(const uint8_t *src, int src_stride,
      uint8_t *dst, int dst_stride, const int16_t *f, int w, int h, int avg)
{
   const int16x4_t flo = vld1_s16(f);
   const int16x4_t fhi = vld1_s16(f + 4);
   int x, y;

   for (x = 0; x + 8 <= w; x += 8)
   {
      const uint8_t *s = src + x;
      uint8_t *dp      = dst + x;
      for (y = 0; y < h; y++)
      {
         int16x8_t r0 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s + 0*src_stride)));
         int16x8_t r1 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s + 1*src_stride)));
         int16x8_t r2 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s + 2*src_stride)));
         int16x8_t r3 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s + 3*src_stride)));
         int16x8_t r4 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s + 4*src_stride)));
         int16x8_t r5 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s + 5*src_stride)));
         int16x8_t r6 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s + 6*src_stride)));
         int16x8_t r7 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s + 7*src_stride)));
         int32x4_t a_lo, a_hi;
         uint8x8_t r8;

         a_lo = vmull_lane_s16(vget_low_s16(r0), flo, 0);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(r1), flo, 1);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(r2), flo, 2);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(r3), flo, 3);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(r4), fhi, 0);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(r5), fhi, 1);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(r6), fhi, 2);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(r7), fhi, 3);
         a_hi = vmull_lane_s16(vget_high_s16(r0), flo, 0);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(r1), flo, 1);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(r2), flo, 2);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(r3), flo, 3);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(r4), fhi, 0);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(r5), fhi, 1);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(r6), fhi, 2);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(r7), fhi, 3);

         r8 = vqmovun_s16(vcombine_s16(vqrshrn_n_s32(a_lo, 7),
                                       vqrshrn_n_s32(a_hi, 7)));
         if (avg)
            r8 = vrhadd_u8(r8, vld1_u8(dp));
         vst1_u8(dp, r8);
         s  += src_stride;
         dp += dst_stride;
      }
   }
   if (x + 4 <= w)
   {
      const uint8_t *s = src + x;
      uint8_t *dp      = dst + x;
      for (y = 0; y < h; y++)
      {
         uint8_t tmp[8];
         int16x8_t r0, r1, r2, r3, r4, r5, r6, r7;
         int32x4_t a_lo;
         uint8x8_t r8;
#define RVP9_NLD4(K) \
         (memcpy(tmp, s + (K)*src_stride, 4), memset(tmp + 4, 0, 4), \
          vreinterpretq_s16_u16(vmovl_u8(vld1_u8(tmp))))
         r0 = RVP9_NLD4(0); r1 = RVP9_NLD4(1);
         r2 = RVP9_NLD4(2); r3 = RVP9_NLD4(3);
         r4 = RVP9_NLD4(4); r5 = RVP9_NLD4(5);
         r6 = RVP9_NLD4(6); r7 = RVP9_NLD4(7);
#undef RVP9_NLD4
         a_lo = vmull_lane_s16(vget_low_s16(r0), flo, 0);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(r1), flo, 1);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(r2), flo, 2);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(r3), flo, 3);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(r4), fhi, 0);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(r5), fhi, 1);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(r6), fhi, 2);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(r7), fhi, 3);
         {
            int16x4_t n16 = vqrshrn_n_s32(a_lo, 7);
            r8 = vqmovun_s16(vcombine_s16(n16, n16));
         }
         if (avg)
         {
            memcpy(tmp, dp, 4);
            memset(tmp + 4, 0, 4);
            r8 = vrhadd_u8(r8, vld1_u8(tmp));
         }
         vst1_u8(tmp, r8);
         memcpy(dp, tmp, 4);
         s  += src_stride;
         dp += dst_stride;
      }
      x += 4;
   }
   for (; x < w; x++)
   {
      const uint8_t *s = src + x;
      uint8_t *dp      = dst + x;
      for (y = 0; y < h; y++)
      {
         int k, sum = 0;
         for (k = 0; k < 8; k++)
            sum += s[k * src_stride] * f[k];
         if (avg)
            *dp = (uint8_t)ROUND_POWER_OF_TWO(*dp +
                  rvp9_clip8(ROUND_POWER_OF_TWO(sum, 7)), 1);
         else
            *dp = rvp9_clip8(ROUND_POWER_OF_TWO(sum, 7));
         s  += src_stride;
         dp += dst_stride;
      }
   }
}
#endif

static void rvp9_convolve_horiz(const RVP9_PIXEL *src, int src_stride,
      RVP9_PIXEL *dst, int dst_stride, const int16_t (*xf)[8],
      int x0_q4, int x_step_q4, int w, int h, int avg)
{
   int x, y;
   src -= RVP9_SUBPEL_TAPS / 2 - 1;
#if defined(RVP9_SSE2) && !RVP9_RECON_HBD
   if (x_step_q4 == 16)
   {
      rvp9_convolve_horiz_sse2(src + (x0_q4 >> RVP9_SUBPEL_BITS),
            src_stride, dst, dst_stride,
            xf[x0_q4 & RVP9_SUBPEL_MASK], w, h, avg);
      return;
   }
#elif defined(RVP9_NEON) && !RVP9_RECON_HBD
   if (x_step_q4 == 16)
   {
      rvp9_convolve_horiz_neon(src + (x0_q4 >> RVP9_SUBPEL_BITS),
            src_stride, dst, dst_stride,
            xf[x0_q4 & RVP9_SUBPEL_MASK], w, h, avg);
      return;
   }
#endif
#if RVP9_RECON_HBD && defined(RVP9_SSE2)
   if (x_step_q4 == 16)
   {
      rvp9_convolve_horiz_hbd_sse2(src + (x0_q4 >> RVP9_SUBPEL_BITS),
            src_stride, dst, dst_stride,
            xf[x0_q4 & RVP9_SUBPEL_MASK], w, h, avg);
      return;
   }
#endif
   for (y = 0; y < h; y++)
   {
      int x_q4 = x0_q4;
      for (x = 0; x < w; x++)
      {
         const RVP9_PIXEL *s = &src[x_q4 >> RVP9_SUBPEL_BITS];
         const int16_t *f = xf[x_q4 & RVP9_SUBPEL_MASK];
         int k, sum = 0;
         for (k = 0; k < RVP9_SUBPEL_TAPS; k++) sum += s[k] * f[k];
         if (avg)
            dst[x] = (RVP9_PIXEL)ROUND_POWER_OF_TWO(dst[x] +
               rvp9_clip8(ROUND_POWER_OF_TWO(sum, RVP9_FILTER_BITS)), 1);
         else
            dst[x] = rvp9_clip8(ROUND_POWER_OF_TWO(sum, RVP9_FILTER_BITS));
         x_q4 += x_step_q4;
      }
      src += src_stride;
      dst += dst_stride;
   }
}

static void rvp9_convolve_vert(const RVP9_PIXEL *src, int src_stride,
      RVP9_PIXEL *dst, int dst_stride, const int16_t (*yf)[8],
      int y0_q4, int y_step_q4, int w, int h, int avg)
{
   int x, y;
   src -= src_stride * (RVP9_SUBPEL_TAPS / 2 - 1);
#if defined(RVP9_SSE2) && !RVP9_RECON_HBD
   if (y_step_q4 == 16)
   {
      rvp9_convolve_vert_sse2(src +
            (y0_q4 >> RVP9_SUBPEL_BITS) * src_stride,
            src_stride, dst, dst_stride,
            yf[y0_q4 & RVP9_SUBPEL_MASK], w, h, avg);
      return;
   }
#elif defined(RVP9_NEON) && !RVP9_RECON_HBD
   if (y_step_q4 == 16)
   {
      rvp9_convolve_vert_neon(src +
            (y0_q4 >> RVP9_SUBPEL_BITS) * src_stride,
            src_stride, dst, dst_stride,
            yf[y0_q4 & RVP9_SUBPEL_MASK], w, h, avg);
      return;
   }
#endif
#if RVP9_RECON_HBD && defined(RVP9_SSE2)
   if (y_step_q4 == 16)
   {
      rvp9_convolve_vert_hbd_sse2(src +
            (y0_q4 >> RVP9_SUBPEL_BITS) * src_stride,
            src_stride, dst, dst_stride,
            yf[y0_q4 & RVP9_SUBPEL_MASK], w, h, avg);
      return;
   }
#endif
   for (x = 0; x < w; x++)
   {
      int y_q4 = y0_q4;
      for (y = 0; y < h; y++)
      {
         const RVP9_PIXEL *s = &src[(y_q4 >> RVP9_SUBPEL_BITS) * src_stride];
         const int16_t *f = yf[y_q4 & RVP9_SUBPEL_MASK];
         int k, sum = 0;
         for (k = 0; k < RVP9_SUBPEL_TAPS; k++)
            sum += s[k * src_stride] * f[k];
         if (avg)
            dst[y * dst_stride] = (RVP9_PIXEL)ROUND_POWER_OF_TWO(
               dst[y * dst_stride] +
               rvp9_clip8(ROUND_POWER_OF_TWO(sum, RVP9_FILTER_BITS)), 1);
         else
            dst[y * dst_stride] =
               rvp9_clip8(ROUND_POWER_OF_TWO(sum, RVP9_FILTER_BITS));
         y_q4 += y_step_q4;
      }
      src++;
      dst++;
   }
}

/* vpx_convolve8_c: horiz into temp, then vert. */
static void rvp9_convolve8(const RVP9_PIXEL *src, int src_stride, RVP9_PIXEL *dst,
      int dst_stride, const int16_t (*kernel)[8], int x0_q4, int x_step_q4,
      int y0_q4, int y_step_q4, int w, int h, int avg)
{
   RVP9_PIXEL temp[64 * 135];
   const int ih =
      (((h - 1) * y_step_q4 + y0_q4) >> RVP9_SUBPEL_BITS) + RVP9_SUBPEL_TAPS;
   rvp9_convolve_horiz(src - src_stride * (RVP9_SUBPEL_TAPS / 2 - 1),
      src_stride, temp, 64, kernel, x0_q4, x_step_q4, w, ih, 0);
   rvp9_convolve_vert(temp + 64 * (RVP9_SUBPEL_TAPS / 2 - 1), 64, dst,
      dst_stride, kernel, y0_q4, y_step_q4, w, h, avg);
}

/* sf->predict dispatch for the unscaled case (xs = ys = 16). */
static void rvp9_inter_predict(const RVP9_PIXEL *src, int src_stride,
      RVP9_PIXEL *dst, int dst_stride, int subpel_x, int subpel_y,
      int w, int h, int ref, const int16_t (*kernel)[8], int xs, int ys)
{
   if (subpel_x && subpel_y)
      rvp9_convolve8(src, src_stride, dst, dst_stride, kernel,
         subpel_x, xs, subpel_y, ys, w, h, ref);
   else if (subpel_x)
      rvp9_convolve_horiz(src, src_stride, dst, dst_stride, kernel,
         subpel_x, xs, w, h, ref);
   else if (subpel_y)
      rvp9_convolve_vert(src, src_stride, dst, dst_stride, kernel,
         subpel_y, ys, w, h, ref);
   else if (ref)
   {
      int x, y;
      for (y = 0; y < h; y++)
         for (x = 0; x < w; x++)
            dst[y * dst_stride + x] = (RVP9_PIXEL)ROUND_POWER_OF_TWO(
               dst[y * dst_stride + x] + src[y * src_stride + x], 1);
   }
   else
   {
      int y;
      for (y = 0; y < h; y++)
         RVP9_PXCOPY(dst + y * dst_stride, src + y * src_stride, w);
   }
}

/* build_mc_border: replicate frame edges into a temp block. */
static void rvp9_build_mc_border(const RVP9_PIXEL *src, int src_stride,
      RVP9_PIXEL *dst, int dst_stride, int x, int y, int b_w, int b_h,
      int w, int h)
{
   const RVP9_PIXEL *ref_row = src - x - y * src_stride;

   if (y >= h)
      ref_row += (h - 1) * src_stride;
   else if (y > 0)
      ref_row += y * src_stride;

   do
   {
      int right = 0, copy;
      int left = x < 0 ? -x : 0;

      if (left > b_w) left = b_w;
      if (x + b_w > w) right = x + b_w - w;
      if (right > b_w) right = b_w;
      copy = b_w - left - right;

      if (left) RVP9_PXFILL(dst, ref_row[0], left);
      if (copy) RVP9_PXCOPY(dst + left, ref_row + x + left, copy);
      if (right) RVP9_PXFILL(dst + left + copy, ref_row[w - 1], right);

      dst += dst_stride;
      ++y;
      if (y > 0 && y < h) ref_row += src_stride;
   } while (--b_h);
}

/* dec_build_inter_predictors, unscaled path only (refs same size). */
static void rvp9_build_inter_pred(rvp9_dec *d, int plane, int x, int y,
      int w, int h, const rvp9_mv *mv, const rvp9_fb *ref_fb,
      const int16_t (*kernel)[8], int ref, RVP9_PIXEL *dst_base, int dst_stride,
      int mi_row, int mi_col)
{
   int ss = plane ? 1 : 0;
   RVP9_PIXEL *dst = dst_base + dst_stride * y + x;
   const RVP9_PIXEL *ref_frame = (const RVP9_PIXEL *)
      (plane == 0 ? ref_fb->y : (plane == 1 ? ref_fb->u : ref_fb->v));
   int frame_width  = plane ? (ref_fb->w + 1) >> 1 : ref_fb->w;
   int frame_height = plane ? (ref_fb->h + 1) >> 1 : ref_fb->h;
   int buf_stride   = plane ? d->uvs : d->ys;
   int x0, y0, x0_16, y0_16, subpel_x, subpel_y;
   rvp9_mv scaled;
   const RVP9_PIXEL *buf_ptr;

   x0 = ((mi_col * 8) >> ss) + x;
   y0 = ((mi_row * 8) >> ss) + y;
   x0_16 = x0 << RVP9_SUBPEL_BITS;
   y0_16 = y0 << RVP9_SUBPEL_BITS;

   scaled.row = (int16_t)(mv->row * (1 << (1 - ss)));
   scaled.col = (int16_t)(mv->col * (1 << (1 - ss)));

   subpel_x = scaled.col & RVP9_SUBPEL_MASK;
   subpel_y = scaled.row & RVP9_SUBPEL_MASK;
   x0 += scaled.col >> RVP9_SUBPEL_BITS;
   y0 += scaled.row >> RVP9_SUBPEL_BITS;
   x0_16 += scaled.col;
   y0_16 += scaled.row;

   buf_ptr = ref_frame + y0 * buf_stride + x0;

   if (scaled.col || scaled.row ||
       (frame_width & 0x7) || (frame_height & 0x7))
   {
      int y1 = ((y0_16 + (h - 1) * 16) >> RVP9_SUBPEL_BITS) + 1;
      int x1 = ((x0_16 + (w - 1) * 16) >> RVP9_SUBPEL_BITS) + 1;
      int x_pad = 0, y_pad = 0;

      if (subpel_x)
      {
         x0 -= RVP9_INTERP_EXTEND - 1;
         x1 += RVP9_INTERP_EXTEND;
         x_pad = 1;
      }
      if (subpel_y)
      {
         y0 -= RVP9_INTERP_EXTEND - 1;
         y1 += RVP9_INTERP_EXTEND;
         y_pad = 1;
      }

      if (x0 < 0 || x0 > frame_width - 1 || x1 < 0 || x1 > frame_width - 1 ||
          y0 < 0 || y0 > frame_height - 1 || y1 < 0 || y1 > frame_height - 1)
      {
         const RVP9_PIXEL *buf_ptr1 = ref_frame + y0 * buf_stride + x0;
         const int b_w = x1 - x0 + 1;
         const int b_h = y1 - y0 + 1;
         const int border_offset = y_pad * 3 * b_w + x_pad * 3;
         RVP9_PIXEL mc_buf[80 * 2 * 80 * 2];

         rvp9_build_mc_border(buf_ptr1, buf_stride, mc_buf, b_w,
            x0, y0, b_w, b_h, frame_width, frame_height);
         rvp9_inter_predict(mc_buf + border_offset, b_w, dst, dst_stride,
            subpel_x, subpel_y, w, h, ref, kernel, 16, 16);
         return;
      }
   }
   rvp9_inter_predict(buf_ptr, buf_stride, dst, dst_stride, subpel_x,
      subpel_y, w, h, ref, kernel, 16, 16);
}

/* average_split_mvs (vp9_reconinter.c): chroma MV for sub-8x8. */
static rvp9_mv rvp9_average_split_mvs(const rvp9_mi *mi, int ref, int ss,
      int block)
{
   rvp9_mv res;
   if (!ss)
      return mi->bmv[block][ref];
   /* 4:2:0 chroma: average all four (round_mv 4) */
   {
      int r = mi->bmv[0][ref].row + mi->bmv[1][ref].row +
              mi->bmv[2][ref].row + mi->bmv[3][ref].row;
      int c = mi->bmv[0][ref].col + mi->bmv[1][ref].col +
              mi->bmv[2][ref].col + mi->bmv[3][ref].col;
      res.row = (int16_t)((r < 0 ? r - 2 : r + 2) / 4);
      res.col = (int16_t)((c < 0 ? c - 2 : c + 2) / 4);
   }
   return res;
}

/* ==================================================================== */
/* Loop filter (vp9_loopfilter.c port): mask building + application.    */
/* ==================================================================== */
#define RVP9_MAX_LOOP_FILTER 63

#if !RVP9_RECON_HBD  /* shared, bit-depth independent types */
typedef struct
{
   uint8_t mblim, lim, hev_thr;
} rvp9_lfthr;

typedef struct
{
   uint64_t left_y[4], above_y[4], int_4x4_y;
   uint16_t left_uv[4], above_uv[4], int_4x4_uv;
   uint8_t  lfl_y[64];
} rvp9_lfm;
#endif /* !RVP9_RECON_HBD */

/* mask LUTs (vp9_loopfilter.c) */
static const uint64_t rvp9_left_txf_mask[4] = {
   0xffffffffffffffffULL, 0xffffffffffffffffULL,
   0x5555555555555555ULL, 0x1111111111111111ULL
};
static const uint64_t rvp9_above_txf_mask[4] = {
   0xffffffffffffffffULL, 0xffffffffffffffffULL,
   0x00ff00ff00ff00ffULL, 0x000000ff000000ffULL
};
static const uint64_t rvp9_left_pred_mask[13] = {
   0x1ULL,0x1ULL,0x1ULL,0x1ULL,0x101ULL,0x1ULL,0x101ULL,0x1010101ULL,
   0x101ULL,0x1010101ULL,0x0101010101010101ULL,0x1010101ULL,
   0x0101010101010101ULL
};
static const uint64_t rvp9_above_pred_mask[13] = {
   0x1ULL,0x1ULL,0x1ULL,0x1ULL,0x1ULL,0x3ULL,0x3ULL,0x3ULL,
   0xfULL,0xfULL,0xfULL,0xffULL,0xffULL
};
static const uint64_t rvp9_size_mask[13] = {
   0x1ULL,0x1ULL,0x1ULL,0x1ULL,0x101ULL,0x3ULL,0x303ULL,0x3030303ULL,
   0xf0fULL,0xf0f0f0fULL,0x0f0f0f0f0f0f0f0fULL,0xffffffffULL,
   0xffffffffffffffffULL
};
static const uint16_t rvp9_left_txf_mask_uv[4]  = {0xffff,0xffff,0x5555,0x1111};
static const uint16_t rvp9_above_txf_mask_uv[4] = {0xffff,0xffff,0x0f0f,0x000f};
static const uint16_t rvp9_left_pred_mask_uv[13] = {
   0x1,0x1,0x1,0x1,0x1,0x1,0x1,0x11,0x1,0x11,0x1111,0x11,0x1111
};
static const uint16_t rvp9_above_pred_mask_uv[13] = {
   0x1,0x1,0x1,0x1,0x1,0x1,0x1,0x1,0x3,0x3,0x3,0xf,0xf
};
static const uint16_t rvp9_size_mask_uv[13] = {
   0x1,0x1,0x1,0x1,0x1,0x1,0x1,0x11,0x3,0x33,0x3333,0xff,0xffff
};

#if !RVP9_RECON_HBD  /* shared, bit-depth independent types */
typedef struct
{
   rvp9_lfthr lfthr[RVP9_MAX_LOOP_FILTER + 1];
   uint8_t    lvl[4][2];    /* [ref_frame][mode_lf_lut]; seg off      */
} rvp9_lf_info;
#endif /* !RVP9_RECON_HBD */

/* mode_lf_lut: intra modes + ZEROMV -> 0; NEAREST/NEAR/NEW -> 1 */
static const uint8_t rvp9_mode_lf_lut[14] =
   { 0,0,0,0,0,0,0,0,0,0, 1,1,0,1 };

static void rvp9_lf_init(rvp9_lf_info *lfi, const rvp9_hdr *hd,
      const int *ref_deltas, const int *mode_deltas)
{
   int lvl;
   int sharpness = hd->lf_sharpness;
   int scale = 1 << (hd->lf_level >> 5);
   int intra_lvl;
   /* update_sharpness */
   for (lvl = 0; lvl <= RVP9_MAX_LOOP_FILTER; lvl++)
   {
      int block_inside_limit =
         lvl >> ((sharpness > 0) + (sharpness > 4));
      if (sharpness > 0 && block_inside_limit > 9 - sharpness)
         block_inside_limit = 9 - sharpness;
      if (block_inside_limit < 1) block_inside_limit = 1;
      lfi->lfthr[lvl].lim   = (uint8_t)block_inside_limit;
      lfi->lfthr[lvl].mblim = (uint8_t)(2 * (lvl + 2) + block_inside_limit);
      lfi->lfthr[lvl].hev_thr = (uint8_t)(lvl >> 4);
   }
   /* filter levels (seg off): lvl[ref][mode] */
   if (hd->lf_delta_enabled)
   {
      int ref, mode;
      intra_lvl = hd->lf_level + ref_deltas[0] * scale;
      lfi->lvl[0][0] = (uint8_t)rvp9_clamp(intra_lvl, 0,
         RVP9_MAX_LOOP_FILTER);
      lfi->lvl[0][1] = lfi->lvl[0][0];
      for (ref = 1; ref < 4; ref++)
         for (mode = 0; mode < 2; mode++)
         {
            int lvl2 = hd->lf_level + ref_deltas[ref] * scale +
               mode_deltas[mode] * scale;
            lfi->lvl[ref][mode] = (uint8_t)rvp9_clamp(lvl2, 0,
               RVP9_MAX_LOOP_FILTER);
         }
   }
   else
      memset(lfi->lvl, hd->lf_level, sizeof lfi->lvl);
}

static void rvp9_build_masks(const rvp9_lf_info *lfi,
      const rvp9_mi *mi, int shift_y, int shift_uv, rvp9_lfm *lfm)
{
   int bs = mi->sb_type;
   int tx_y = mi->tx_size;
   int ssb = bs < 3 ? 0 : rvp9_ss_size[bs];
   int mt = rvp9_max_tx[ssb];
   int tx_uv = tx_y < mt ? tx_y : mt;
   int filter_level =
      lfi->lvl[mi->ref_frame[0] > 0 ? mi->ref_frame[0] : 0]
              [rvp9_mode_lf_lut[mi->mode]];
   int i;

   if (!filter_level)
      return;
   else
   {
      const int w = 1 << rvp9_mi_w_l2[bs];
      const int h = 1 << rvp9_mi_h_l2[bs];
      int index = shift_y;
      for (i = 0; i < h; i++)
      {
         memset(&lfm->lfl_y[index], filter_level, w);
         index += 8;
      }
   }

   lfm->above_y[tx_y]   |= rvp9_above_pred_mask[bs] << shift_y;
   lfm->above_uv[tx_uv] |= (uint16_t)(rvp9_above_pred_mask_uv[bs] << shift_uv);
   lfm->left_y[tx_y]    |= rvp9_left_pred_mask[bs] << shift_y;
   lfm->left_uv[tx_uv]  |= (uint16_t)(rvp9_left_pred_mask_uv[bs] << shift_uv);

   if (mi->skip && rvp9_is_inter(mi)) return;

   lfm->above_y[tx_y] |= (rvp9_size_mask[bs] & rvp9_above_txf_mask[tx_y])
                         << shift_y;
   lfm->above_uv[tx_uv] |= (uint16_t)((rvp9_size_mask_uv[bs] &
                            rvp9_above_txf_mask_uv[tx_uv]) << shift_uv);
   lfm->left_y[tx_y] |= (rvp9_size_mask[bs] & rvp9_left_txf_mask[tx_y])
                        << shift_y;
   lfm->left_uv[tx_uv] |= (uint16_t)((rvp9_size_mask_uv[bs] &
                           rvp9_left_txf_mask_uv[tx_uv]) << shift_uv);

   if (tx_y == 0)  lfm->int_4x4_y  |= rvp9_size_mask[bs] << shift_y;
   if (tx_uv == 0) lfm->int_4x4_uv |= (uint16_t)((rvp9_size_mask_uv[bs]
                                       & 0xffff) << shift_uv);
}

static void rvp9_build_y_mask(const rvp9_lf_info *lfi,
      const rvp9_mi *mi, int shift_y, rvp9_lfm *lfm)
{
   int bs = mi->sb_type;
   int tx_y = mi->tx_size;
   int filter_level =
      lfi->lvl[mi->ref_frame[0] > 0 ? mi->ref_frame[0] : 0]
              [rvp9_mode_lf_lut[mi->mode]];
   int i;

   if (!filter_level)
      return;
   else
   {
      const int w = 1 << rvp9_mi_w_l2[bs];
      const int h = 1 << rvp9_mi_h_l2[bs];
      int index = shift_y;
      for (i = 0; i < h; i++)
      {
         memset(&lfm->lfl_y[index], filter_level, w);
         index += 8;
      }
   }

   lfm->above_y[tx_y] |= rvp9_above_pred_mask[bs] << shift_y;
   lfm->left_y[tx_y]  |= rvp9_left_pred_mask[bs] << shift_y;

   if (mi->skip && rvp9_is_inter(mi)) return;

   lfm->above_y[tx_y] |= (rvp9_size_mask[bs] & rvp9_above_txf_mask[tx_y])
                         << shift_y;
   lfm->left_y[tx_y]  |= (rvp9_size_mask[bs] & rvp9_left_txf_mask[tx_y])
                         << shift_y;
   if (tx_y == 0) lfm->int_4x4_y |= rvp9_size_mask[bs] << shift_y;
}

static void rvp9_setup_mask(rvp9_dec *d, const rvp9_lf_info *lfi,
      int mi_row, int mi_col, rvp9_lfm *lfm)
{
   int idx_32, idx_16, idx_8;
   const rvp9_mi *mip = d->mi + mi_row * d->mi_cols + mi_col;
   const rvp9_mi *mip2;
   const int stride = d->mi_cols;
   int offset_32[4];
   int offset_16[4];
   int offset_8[4];
   static const int shift_32_y[4]  = { 0, 4, 32, 36 };
   static const int shift_16_y[4]  = { 0, 2, 16, 18 };
   static const int shift_8_y[4]   = { 0, 1, 8, 9 };
   static const int shift_32_uv[4] = { 0, 2, 8, 10 };
   static const int shift_16_uv[4] = { 0, 1, 4, 5 };
   const int max_rows = (mi_row + 8 > d->mi_rows ? d->mi_rows - mi_row : 8);
   const int max_cols = (mi_col + 8 > d->mi_cols ? d->mi_cols - mi_col : 8);

   offset_32[0] = 4; offset_32[1] = (stride << 2) - 4;
   offset_32[2] = 4; offset_32[3] = -(stride << 2) - 4;
   offset_16[0] = 2; offset_16[1] = (stride << 1) - 2;
   offset_16[2] = 2; offset_16[3] = -(stride << 1) - 2;
   offset_8[0]  = 1; offset_8[1]  = stride - 1;
   offset_8[2]  = 1; offset_8[3]  = -stride - 1;

   memset(lfm, 0, sizeof *lfm);

   switch (mip->sb_type)
   {
      case 12: rvp9_build_masks(lfi, mip, 0, 0, lfm); break;
      case 11:
         rvp9_build_masks(lfi, mip, 0, 0, lfm);
         if (4 >= max_rows) break;
         mip2 = mip + stride * 4;
         rvp9_build_masks(lfi, mip2, 32, 8, lfm);
         break;
      case 10:
         rvp9_build_masks(lfi, mip, 0, 0, lfm);
         if (4 >= max_cols) break;
         mip2 = mip + 4;
         rvp9_build_masks(lfi, mip2, 4, 2, lfm);
         break;
      default:
         for (idx_32 = 0; idx_32 < 4; mip += offset_32[idx_32], ++idx_32)
         {
            const int shift_y_32 = shift_32_y[idx_32];
            const int shift_uv_32 = shift_32_uv[idx_32];
            const int mi_32_col_offset = (idx_32 & 1) << 2;
            const int mi_32_row_offset = (idx_32 >> 1) << 2;
            if (mi_32_col_offset >= max_cols || mi_32_row_offset >= max_rows)
               continue;
            switch (mip->sb_type)
            {
               case 9: rvp9_build_masks(lfi, mip, shift_y_32, shift_uv_32, lfm); break;
               case 8:
                  rvp9_build_masks(lfi, mip, shift_y_32, shift_uv_32, lfm);
                  if (mi_32_row_offset + 2 >= max_rows) continue;
                  mip2 = mip + stride * 2;
                  rvp9_build_masks(lfi, mip2, shift_y_32 + 16, shift_uv_32 + 4, lfm);
                  break;
               case 7:
                  rvp9_build_masks(lfi, mip, shift_y_32, shift_uv_32, lfm);
                  if (mi_32_col_offset + 2 >= max_cols) continue;
                  mip2 = mip + 2;
                  rvp9_build_masks(lfi, mip2, shift_y_32 + 2, shift_uv_32 + 1, lfm);
                  break;
               default:
                  for (idx_16 = 0; idx_16 < 4; mip += offset_16[idx_16], ++idx_16)
                  {
                     const int shift_y_16 = shift_y_32 + shift_16_y[idx_16];
                     const int shift_uv_16 = shift_uv_32 + shift_16_uv[idx_16];
                     const int mi_16_col_offset = mi_32_col_offset + ((idx_16 & 1) << 1);
                     const int mi_16_row_offset = mi_32_row_offset + ((idx_16 >> 1) << 1);
                     if (mi_16_col_offset >= max_cols || mi_16_row_offset >= max_rows)
                        continue;
                     switch (mip->sb_type)
                     {
                        case 6: rvp9_build_masks(lfi, mip, shift_y_16, shift_uv_16, lfm); break;
                        case 5:
                           rvp9_build_masks(lfi, mip, shift_y_16, shift_uv_16, lfm);
                           if (mi_16_row_offset + 1 >= max_rows) continue;
                           mip2 = mip + stride;
                           rvp9_build_y_mask(lfi, mip2, shift_y_16 + 8, lfm);
                           break;
                        case 4:
                           rvp9_build_masks(lfi, mip, shift_y_16, shift_uv_16, lfm);
                           if (mi_16_col_offset + 1 >= max_cols) continue;
                           mip2 = mip + 1;
                           rvp9_build_y_mask(lfi, mip2, shift_y_16 + 1, lfm);
                           break;
                        default:
                        {
                           const int shift_y_8_0 = shift_y_16 + shift_8_y[0];
                           rvp9_build_masks(lfi, mip, shift_y_8_0, shift_uv_16, lfm);
                           mip += offset_8[0];
                           for (idx_8 = 1; idx_8 < 4; mip += offset_8[idx_8], ++idx_8)
                           {
                              const int shift_y_8 = shift_y_16 + shift_8_y[idx_8];
                              const int mi_8_col_offset = mi_16_col_offset + (idx_8 & 1);
                              const int mi_8_row_offset = mi_16_row_offset + (idx_8 >> 1);
                              if (mi_8_col_offset >= max_cols ||
                                  mi_8_row_offset >= max_rows)
                                 continue;
                              rvp9_build_y_mask(lfi, mip, shift_y_8, lfm);
                           }
                           break;
                        }
                     }
                  }
                  break;
            }
         }
         break;
   }
}

static void rvp9_adjust_mask(const rvp9_dec *d, int mi_row, int mi_col,
      rvp9_lfm *lfm)
{
   static const uint64_t left_border  = 0x1111111111111111ULL;
   static const uint64_t above_border = 0x000000ff000000ffULL;
   static const uint16_t left_border_uv  = 0x1111;
   static const uint16_t above_border_uv = 0x000f;
   int i;

   lfm->left_y[2]   |= lfm->left_y[3];
   lfm->above_y[2]  |= lfm->above_y[3];
   lfm->left_uv[2]  |= lfm->left_uv[3];
   lfm->above_uv[2] |= lfm->above_uv[3];

   lfm->left_y[1] |= lfm->left_y[0] & left_border;
   lfm->left_y[0] &= ~left_border;
   lfm->above_y[1] |= lfm->above_y[0] & above_border;
   lfm->above_y[0] &= ~above_border;
   lfm->left_uv[1] |= lfm->left_uv[0] & left_border_uv;
   lfm->left_uv[0] &= (uint16_t)~left_border_uv;
   lfm->above_uv[1] |= lfm->above_uv[0] & above_border_uv;
   lfm->above_uv[0] &= (uint16_t)~above_border_uv;

   if (mi_row + 8 > d->mi_rows)
   {
      const uint64_t rows = (uint64_t)(d->mi_rows - mi_row);
      const uint64_t mask_y = ((uint64_t)1 << (rows << 3)) - 1;
      const uint16_t mask_uv =
         (uint16_t)((1 << ((((int)rows + 1) >> 1) << 2)) - 1);

      for (i = 0; i < 3; i++)
      {
         lfm->left_y[i]   &= mask_y;
         lfm->above_y[i]  &= mask_y;
         lfm->left_uv[i]  &= mask_uv;
         lfm->above_uv[i] &= mask_uv;
      }
      lfm->int_4x4_y  &= mask_y;
      lfm->int_4x4_uv &= mask_uv;

      if (rows == 1)
      {
         lfm->above_uv[1] |= lfm->above_uv[2];
         lfm->above_uv[2] = 0;
      }
      if (rows == 5)
      {
         lfm->above_uv[1] |= lfm->above_uv[2] & 0xff00;
         lfm->above_uv[2] &= (uint16_t)~(lfm->above_uv[2] & 0xff00);
      }
   }

   if (mi_col + 8 > d->mi_cols)
   {
      const int columns = d->mi_cols - mi_col;
      const uint64_t mask_y = ((1ULL << columns) - 1) * 0x0101010101010101ULL;
      const uint16_t mask_uv =
         (uint16_t)(((1 << ((columns + 1) >> 1)) - 1) * 0x1111);
      const uint16_t mask_uv_int =
         (uint16_t)(((1 << (columns >> 1)) - 1) * 0x1111);

      for (i = 0; i < 3; i++)
      {
         lfm->left_y[i]   &= mask_y;
         lfm->above_y[i]  &= mask_y;
         lfm->left_uv[i]  &= mask_uv;
         lfm->above_uv[i] &= mask_uv;
      }
      lfm->int_4x4_y  &= mask_y;
      lfm->int_4x4_uv &= mask_uv_int;

      if (columns == 1)
      {
         lfm->left_uv[1] |= lfm->left_uv[2];
         lfm->left_uv[2] = 0;
      }
      if (columns == 5)
      {
         lfm->left_uv[1] |= lfm->left_uv[2] & 0xcccc;
         lfm->left_uv[2] &= (uint16_t)~(lfm->left_uv[2] & 0xcccc);
      }
   }

   if (mi_col == 0)
      for (i = 0; i < 3; i++)
      {
         lfm->left_y[i]  &= 0xfefefefefefefefeULL;
         lfm->left_uv[i] &= 0xeeee;
      }
}

/* Simplified per-position application (equivalent to the dual variants). */
static void rvp9_filter_vert_line(RVP9_PIXEL *s, int pitch, unsigned m16,
      unsigned m8, unsigned m4, unsigned m4i, const rvp9_lfthr *thr,
      const uint8_t *lfl, int positions)
{
   int c;
   for (c = 0; c < positions; c++)
   {
      const rvp9_lfthr *t = thr + lfl[c];
      uint8_t mblim[16], lim[16], hev[16];
      memset(mblim, t->mblim, 16); memset(lim, t->lim, 16);
      memset(hev, t->hev_thr, 16);
      if ((m16 >> c) & 1)
         rvp9_lpf_vertical_16(s + 8 * c, pitch, mblim, lim, hev);
      else if ((m8 >> c) & 1)
         rvp9_lpf_vertical_8(s + 8 * c, pitch, mblim, lim, hev);
      else if ((m4 >> c) & 1)
         rvp9_lpf_vertical_4(s + 8 * c, pitch, mblim, lim, hev);
      if ((m4i >> c) & 1)
         rvp9_lpf_vertical_4(s + 8 * c + 4, pitch, mblim, lim, hev);
   }
}

static void rvp9_filter_horiz_line(RVP9_PIXEL *s, int pitch, unsigned m16,
      unsigned m8, unsigned m4, unsigned m4i, const rvp9_lfthr *thr,
      const uint8_t *lfl, int positions)
{
   int c;
   for (c = 0; c < positions; c++)
   {
      const rvp9_lfthr *t = thr + lfl[c];
      uint8_t mblim[16], lim[16], hev[16];
      memset(mblim, t->mblim, 16); memset(lim, t->lim, 16);
      memset(hev, t->hev_thr, 16);
      if ((m16 >> c) & 1)
         rvp9_lpf_horizontal_16(s + 8 * c, pitch, mblim, lim, hev);
      else if ((m8 >> c) & 1)
         rvp9_lpf_horizontal_8(s + 8 * c, pitch, mblim, lim, hev);
      else if ((m4 >> c) & 1)
         rvp9_lpf_horizontal_4(s + 8 * c, pitch, mblim, lim, hev);
      if ((m4i >> c) & 1)
         rvp9_lpf_horizontal_4(s + 8 * c + 4 * pitch, pitch, mblim, lim, hev);
   }
}

static void rvp9_filter_sb(rvp9_dec *d, const rvp9_lf_info *lfi,
      int mi_row, int mi_col, rvp9_lfm *lfm)
{
   int r;
   /* ---- luma (ss00) ---- */
   {
      RVP9_PIXEL *buf = (RVP9_PIXEL*)d->buf_y + (mi_row * 8) * d->ys + mi_col * 8;
      uint64_t m16 = lfm->left_y[2], m8 = lfm->left_y[1];
      uint64_t m4 = lfm->left_y[0], m4i = lfm->int_4x4_y;
      for (r = 0; r < 8 && mi_row + r < d->mi_rows; r++)
      {
         rvp9_filter_vert_line(buf, d->ys,
            (unsigned)(m16 & 0xff), (unsigned)(m8 & 0xff),
            (unsigned)(m4 & 0xff), (unsigned)(m4i & 0xff),
            lfi->lfthr, &lfm->lfl_y[r << 3], 8);
         buf += 8 * d->ys;
         m16 >>= 8; m8 >>= 8; m4 >>= 8; m4i >>= 8;
      }
      buf = (RVP9_PIXEL*)d->buf_y + (mi_row * 8) * d->ys + mi_col * 8;
      m16 = lfm->above_y[2]; m8 = lfm->above_y[1];
      m4 = lfm->above_y[0]; m4i = lfm->int_4x4_y;
      for (r = 0; r < 8 && mi_row + r < d->mi_rows; r++)
      {
         unsigned r16, r8, r4;
         if (mi_row + r == 0) { r16 = r8 = r4 = 0; }
         else
         {
            r16 = (unsigned)(m16 & 0xff);
            r8  = (unsigned)(m8 & 0xff);
            r4  = (unsigned)(m4 & 0xff);
         }
         rvp9_filter_horiz_line(buf, d->ys, r16, r8, r4,
            (unsigned)(m4i & 0xff), lfi->lfthr, &lfm->lfl_y[r << 3], 8);
         buf += 8 * d->ys;
         m16 >>= 8; m8 >>= 8; m4 >>= 8; m4i >>= 8;
      }
   }
   /* ---- chroma (ss11), applied to both u and v ---- */
   {
      int p;
      uint8_t lfl_uv[16];
      int r2, c;
      memset(lfl_uv, 0, sizeof lfl_uv);
      for (r2 = 0; r2 < 8; r2 += 2)
         for (c = 0; c < 4; c++)
            lfl_uv[(r2 << 1) + c] = lfm->lfl_y[(r2 << 3) + (c << 1)];

      for (p = 1; p <= 2; p++)
      {
         RVP9_PIXEL *base = (RVP9_PIXEL*)(p == 1 ? d->buf_u : d->buf_v)
                       + (mi_row * 4) * d->uvs + mi_col * 4;
         RVP9_PIXEL *buf = base;
         uint16_t m16 = lfm->left_uv[2], m8 = lfm->left_uv[1];
         uint16_t m4 = lfm->left_uv[0], m4i = lfm->int_4x4_uv;
         for (r = 0; r < 8 && mi_row + r < d->mi_rows; r += 2)
         {
            rvp9_filter_vert_line(buf, d->uvs,
               (unsigned)(m16 & 0xf), (unsigned)(m8 & 0xf),
               (unsigned)(m4 & 0xf), (unsigned)(m4i & 0xf),
               lfi->lfthr, &lfl_uv[r << 1], 4);
            buf += 8 * d->uvs;
            m16 >>= 4; m8 >>= 4; m4 >>= 4; m4i >>= 4;
         }
         buf = base;
         m16 = lfm->above_uv[2]; m8 = lfm->above_uv[1];
         m4 = lfm->above_uv[0]; m4i = lfm->int_4x4_uv;
         for (r = 0; r < 8 && mi_row + r < d->mi_rows; r += 2)
         {
            const int skip_border = (mi_row + r == d->mi_rows - 1);
            unsigned r4i = skip_border ? 0 : (unsigned)(m4i & 0xf);
            unsigned r16, r8, r4;
            if (mi_row + r == 0) { r16 = r8 = r4 = 0; }
            else
            {
               r16 = (unsigned)(m16 & 0xf);
               r8  = (unsigned)(m8 & 0xf);
               r4  = (unsigned)(m4 & 0xf);
            }
            rvp9_filter_horiz_line(buf, d->uvs, r16, r8, r4, r4i,
               lfi->lfthr, &lfl_uv[r << 1], 4);
            buf += 8 * d->uvs;
            m16 >>= 4; m8 >>= 4; m4 >>= 4; m4i >>= 4;
         }
      }
   }
}

static void rvp9_loop_filter_frame(rvp9_dec *d)
{
   rvp9_lf_info lfi;
   rvp9_lfm lfm;
   int mi_row, mi_col;
   if (!d->hd.lf_level) return;
   rvp9_lf_init(&lfi, &d->hd, d->lf_ref_deltas, d->lf_mode_deltas);
   for (mi_row = 0; mi_row < d->mi_rows; mi_row += 8)
      for (mi_col = 0; mi_col < d->mi_cols; mi_col += 8)
      {
         rvp9_setup_mask(d, &lfi, mi_row, mi_col, &lfm);
         rvp9_adjust_mask(d, mi_row, mi_col, &lfm);
         rvp9_filter_sb(d, &lfi, mi_row, mi_col, &lfm);
      }
}



#endif /* RVP9_RECON_PART == 4 */

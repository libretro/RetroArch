/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef VG_INT_MAT3X3_H
#define VG_INT_MAT3X3_H

typedef struct {
   float m[3][3]; /* [y][x] */
} VG_MAT3X3_T;

extern void vg_mat3x3_set_identity(VG_MAT3X3_T *a);
extern void vg_mat3x3_set_clean(VG_MAT3X3_T *a, const float *matrix, bool force_affine);
extern void vg_mat3x3_get(const VG_MAT3X3_T *a, float *matrix);
extern bool vg_mat3x3_identical(const VG_MAT3X3_T *a, const VG_MAT3X3_T *b);

extern void vg_mat3x3_mul(VG_MAT3X3_T *a, const VG_MAT3X3_T *b, const VG_MAT3X3_T *c);

/*
   Preconditions:

   a and b may point to the same matrix.

   Postconditions:

   a is set to a * b.
*/

static INLINE void vg_mat3x3_postmul(VG_MAT3X3_T *a, const VG_MAT3X3_T *b)
{
   VG_MAT3X3_T c;
   vg_mat3x3_mul(&c, a, b);
   *a = c;
}

extern void vg_mat3x3_postmul_translate(VG_MAT3X3_T *a, float x, float y);
extern void vg_mat3x3_postmul_scale(VG_MAT3X3_T *a, float x, float y);
extern void vg_mat3x3_postmul_shear(VG_MAT3X3_T *a, float x, float y);
extern void vg_mat3x3_postmul_rotate(VG_MAT3X3_T *a, float angle);
extern void vg_mat3x3_postmul_rotate_sc(VG_MAT3X3_T *a, float s, float c);

/*
   Preconditions:

   a and b may point to the same matrix.

   Postconditions:

   a is set to b * a.
*/

static INLINE void vg_mat3x3_premul(VG_MAT3X3_T *a, const VG_MAT3X3_T *b)
{
   VG_MAT3X3_T c;
   vg_mat3x3_mul(&c, b, a);
   *a = c;
}

extern void vg_mat3x3_premul_translate(VG_MAT3X3_T *a, float x, float y);
extern void vg_mat3x3_premul_scale(VG_MAT3X3_T *a, float x, float y);
extern void vg_mat3x3_premul_shear(VG_MAT3X3_T *a, float x, float y);
extern void vg_mat3x3_premul_rotate(VG_MAT3X3_T *a, float angle);
extern void vg_mat3x3_premul_rotate_sc(VG_MAT3X3_T *a, float s, float c);

extern bool vg_mat3x3_is_affine(const VG_MAT3X3_T *a);

/* it is possible to start with an affine matrix, apply only affine-preserving
 * transformations, and then end up with a matrix where vg_mat3x3_is_affine
 * returns false. this is because the bottom row of the matrix can end up
 * containing nans if infinities or nans crop up in the calculations
 *
 * this is why i vcos_assert(vg_mat3x3_is_affine_or_nans) rather than just
 * vcos_assert(vg_mat3x3_is_affine) in various places. todo: i think this could
 * be fixed by having special affine versions of the various multiply functions
 * that don't touch the bottom row at all, but is it worth it? */
extern bool vg_mat3x3_is_affine_or_nans(const VG_MAT3X3_T *a);

extern float vg_mat3x3_det(const VG_MAT3X3_T *a);
extern float vg_mat3x3_affine_det(const VG_MAT3X3_T *a);

extern bool vg_mat3x3_is_invertible(const VG_MAT3X3_T *a);
extern bool vg_mat3x3_affine_is_invertible(const VG_MAT3X3_T *a);

extern void vg_mat3x3_invert(VG_MAT3X3_T *a);
extern void vg_mat3x3_affine_invert(VG_MAT3X3_T *a);

/*
   Preconditions:

   -

   Postconditions:

   if a is invertible, according to vg_mat3x3_is_invertible, true is returned
   and a is inverted. otherwise, false is returned and a is untouched.
*/

static INLINE bool vg_mat3x3_try_invert(VG_MAT3X3_T *a)
{
   if (!vg_mat3x3_is_invertible(a)) {
      return false;
   }
   vg_mat3x3_invert(a);
   return true;
}

/*
   Preconditions:

   a must be affine (or have nans in the bad elements).

   Postconditions:

   if a is invertible, according to vg_mat3x3_affine_is_invertible, true is
   returned and a is inverted. otherwise, false is returned and a is untouched.
*/

static INLINE bool vg_mat3x3_affine_try_invert(VG_MAT3X3_T *a)
{
   if (!vg_mat3x3_affine_is_invertible(a)) {
      return false;
   }
   vg_mat3x3_affine_invert(a);
   return true;
}

extern void vg_mat3x3_affine_transform(const VG_MAT3X3_T *a, float *x, float *y);
extern void vg_mat3x3_affine_transform_t(const VG_MAT3X3_T *a, float *x, float *y);

extern void vg_mat3x3_rsq(const VG_MAT3X3_T *a, /* only top-left 2x2 matrix used */
   float *r, float *s0, float *s1); /* don't need q for anything atm, so not calculated */

#endif

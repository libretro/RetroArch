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

#include "interface/khronos/common/khrn_int_common.h"
#include "interface/khronos/vg/vg_int_mat3x3.h"
#include "interface/khronos/common/khrn_int_math.h"
#include "interface/khronos/common/khrn_int_util.h"

/*
   Preconditions:

   -

   Postconditions:

   a is the identity matrix.
*/

void vg_mat3x3_set_identity(VG_MAT3X3_T *a)
{
   a->m[0][0] = 1.0f;  a->m[0][1] = 0.0f;  a->m[0][2] = 0.0f;
   a->m[1][0] = 0.0f;  a->m[1][1] = 1.0f;  a->m[1][2] = 0.0f;
   a->m[2][0] = 0.0f;  a->m[2][1] = 0.0f;  a->m[2][2] = 1.0f;
}

/*
   copies matrix (column-major) into a, "cleaning" the elements (see
   clean_float). forces the bottom row of a to (0, 0, 1) if force_affine is
   true.

   Preconditions:

   -

   Postconditions:

   no elements of a are nan or infinity.
   if force_affine is true, a is affine.
*/

void vg_mat3x3_set_clean(VG_MAT3X3_T *a, const float *matrix, bool force_affine)
{
   a->m[0][0] = clean_float(matrix[0]);
   a->m[0][1] = clean_float(matrix[3]);
   a->m[0][2] = clean_float(matrix[6]);

   a->m[1][0] = clean_float(matrix[1]);
   a->m[1][1] = clean_float(matrix[4]);
   a->m[1][2] = clean_float(matrix[7]);

   if (force_affine) {
      a->m[2][0] = 0.0f;
      a->m[2][1] = 0.0f;
      a->m[2][2] = 1.0f;
   } else {
      a->m[2][0] = clean_float(matrix[2]);
      a->m[2][1] = clean_float(matrix[5]);
      a->m[2][2] = clean_float(matrix[8]);
   }
}

/*
   Preconditions:

   -

   Postconditions:

   matrix (column-major) is set to a.
*/

void vg_mat3x3_get(const VG_MAT3X3_T *a, float *matrix)
{
   matrix[0] = a->m[0][0];
   matrix[3] = a->m[0][1];
   matrix[6] = a->m[0][2];

   matrix[1] = a->m[1][0];
   matrix[4] = a->m[1][1];
   matrix[7] = a->m[1][2];

   matrix[2] = a->m[2][0];
   matrix[5] = a->m[2][1];
   matrix[8] = a->m[2][2];
}

/*
   Preconditions:

   -

   Postconditions:

   returns true iff a and b are bitwise identical.
*/

bool vg_mat3x3_identical(const VG_MAT3X3_T *a, const VG_MAT3X3_T *b)
{
   return floats_identical(a->m[0][0], b->m[0][0]) && floats_identical(a->m[0][1], b->m[0][1]) && floats_identical(a->m[0][2], b->m[0][2]) &&
          floats_identical(a->m[1][0], b->m[1][0]) && floats_identical(a->m[1][1], b->m[1][1]) && floats_identical(a->m[1][2], b->m[1][2]) &&
          floats_identical(a->m[2][0], b->m[2][0]) && floats_identical(a->m[2][1], b->m[2][1]) && floats_identical(a->m[2][2], b->m[2][2]);
}

/*
   Preconditions:

   a does not point to the same matrix as b or c.

   Postconditions:

   a is set to b * c.
*/

void vg_mat3x3_mul(VG_MAT3X3_T *a, const VG_MAT3X3_T *b, const VG_MAT3X3_T *c)
{
   uint32_t j, i;
   for (j = 0; j != 3; ++j) {
      for (i = 0; i != 3; ++i) {
         a->m[j][i] =
            (b->m[j][0] * c->m[0][i]) +
            (b->m[j][1] * c->m[1][i]) +
            (b->m[j][2] * c->m[2][i]);
      }
   }
}

/*
   Preconditions:

   -

   Postconditions:

   a is set to a * translation_matrix(x, y).
*/

void vg_mat3x3_postmul_translate(VG_MAT3X3_T *a, float x, float y)
{
   a->m[0][2] += (a->m[0][0] * x) + (a->m[0][1] * y);
   a->m[1][2] += (a->m[1][0] * x) + (a->m[1][1] * y);
   a->m[2][2] += (a->m[2][0] * x) + (a->m[2][1] * y);
}

/*
   Preconditions:

   -

   Postconditions:

   a is set to a * scale_matrix(x, y).
*/

void vg_mat3x3_postmul_scale(VG_MAT3X3_T *a, float x, float y)
{
   a->m[0][0] *= x;
   a->m[0][1] *= y;

   a->m[1][0] *= x;
   a->m[1][1] *= y;

   a->m[2][0] *= x;
   a->m[2][1] *= y;
}

/*
   Preconditions:

   -

   Postconditions:

   a is set to a * shear_matrix(x, y).
*/

void vg_mat3x3_postmul_shear(VG_MAT3X3_T *a, float x, float y)
{
   float m00 = a->m[0][0], m10 = a->m[1][0], m20 = a->m[2][0];

   a->m[0][0] += a->m[0][1] * y;
   a->m[0][1] += m00 * x;

   a->m[1][0] += a->m[1][1] * y;
   a->m[1][1] += m10 * x;

   a->m[2][0] += a->m[2][1] * y;
   a->m[2][1] += m20 * x;
}

/*
   Preconditions:

   angle is in radians.

   Postconditions:

   a is set to a * rotation_matrix(angle).
*/

void vg_mat3x3_postmul_rotate(VG_MAT3X3_T *a, float angle)
{
   float s, c;
   sin_cos_(&s, &c, angle);
   vg_mat3x3_postmul_rotate_sc(a, s, c);
}

/*
   Preconditions:

   there is some angle such that:
   s = sin(angle)
   c = cos(angle)

   Postconditions:

   a is set to a * rotation_matrix(angle).
*/

void vg_mat3x3_postmul_rotate_sc(VG_MAT3X3_T *a, float s, float c)
{
   float m00 = a->m[0][0], m10 = a->m[1][0], m20 = a->m[2][0];

   a->m[0][0] = (m00 * c) + (a->m[0][1] * s);
   a->m[0][1] = (a->m[0][1] * c) - (m00 * s);

   a->m[1][0] = (m10 * c) + (a->m[1][1] * s);
   a->m[1][1] = (a->m[1][1] * c) - (m10 * s);

   a->m[2][0] = (m20 * c) + (a->m[2][1] * s);
   a->m[2][1] = (a->m[2][1] * c) - (m20 * s);
}

/*
   Preconditions:

   -

   Postconditions:

   a is set to translation_matrix(x, y) * a.
*/

void vg_mat3x3_premul_translate(VG_MAT3X3_T *a, float x, float y)
{
   a->m[0][0] += a->m[2][0] * x;
   a->m[0][1] += a->m[2][1] * x;
   a->m[0][2] += a->m[2][2] * x;

   a->m[1][0] += a->m[2][0] * y;
   a->m[1][1] += a->m[2][1] * y;
   a->m[1][2] += a->m[2][2] * y;
}

/*
   Preconditions:

   -

   Postconditions:

   a is set to scale_matrix(x, y) * a.
*/

void vg_mat3x3_premul_scale(VG_MAT3X3_T *a, float x, float y)
{
   a->m[0][0] *= x;
   a->m[0][1] *= x;
   a->m[0][2] *= x;

   a->m[1][0] *= y;
   a->m[1][1] *= y;
   a->m[1][2] *= y;
}

/*
   Preconditions:

   -

   Postconditions:

   a is set to shear_matrix(x, y) * a.
*/

void vg_mat3x3_premul_shear(VG_MAT3X3_T *a, float x, float y)
{
   float m00 = a->m[0][0], m01 = a->m[0][1], m02 = a->m[0][2];

   a->m[0][0] += a->m[1][0] * x;
   a->m[0][1] += a->m[1][1] * x;
   a->m[0][2] += a->m[1][2] * x;

   a->m[1][0] += m00 * y;
   a->m[1][1] += m01 * y;
   a->m[1][2] += m02 * y;
}

/*
   Preconditions:

   angle is in radians.

   Postconditions:

   a is set to rotation_matrix(angle) * a.
*/

void vg_mat3x3_premul_rotate(VG_MAT3X3_T *a, float angle)
{
   float s, c;
   sin_cos_(&s, &c, angle);
   vg_mat3x3_premul_rotate_sc(a, s, c);
}

/*
   Preconditions:

   there is some angle such that:
   s = sin(angle)
   c = cos(angle)

   Postconditions:

   a is set to rotation_matrix(angle) * a.
*/

void vg_mat3x3_premul_rotate_sc(VG_MAT3X3_T *a, float s, float c)
{
   float m00 = a->m[0][0], m01 = a->m[0][1], m02 = a->m[0][2];

   a->m[0][0] = (m00 * c) - (a->m[1][0] * s);
   a->m[0][1] = (m01 * c) - (a->m[1][1] * s);
   a->m[0][2] = (m02 * c) - (a->m[1][2] * s);

   a->m[1][0] = (m00 * s) + (a->m[1][0] * c);
   a->m[1][1] = (m01 * s) + (a->m[1][1] * c);
   a->m[1][2] = (m02 * s) + (a->m[1][2] * c);
}

/*
   Preconditions:

   -

   Postconditions:

   returns true iff a is affine.
*/

bool vg_mat3x3_is_affine(const VG_MAT3X3_T *a)
{
   return (a->m[2][0] == 0.0f) && (a->m[2][1] == 0.0f) && (a->m[2][2] == 1.0f);
}

/*
   Preconditions:

   -

   Postconditions:

   returns true iff a is affine or has nans in the bad elements.
*/

bool vg_mat3x3_is_affine_or_nans(const VG_MAT3X3_T *a)
{
   return !nan_ne_(a->m[2][0], 0.0f) && !nan_ne_(a->m[2][1], 0.0f) && !nan_ne_(a->m[2][2], 1.0f);
}

/*
   Preconditions:

   -

   Postconditions:

   returns the determinant of a.
*/

float vg_mat3x3_det(const VG_MAT3X3_T *a)
{
   return (a->m[0][0] * ((a->m[2][2] * a->m[1][1]) - (a->m[2][1] * a->m[1][2]))) +
          (a->m[1][0] * ((a->m[0][2] * a->m[2][1]) - (a->m[0][1] * a->m[2][2]))) +
          (a->m[2][0] * ((a->m[1][2] * a->m[0][1]) - (a->m[1][1] * a->m[0][2])));
}

/*
   Preconditions:

   a must be affine (or have nans in the bad elements).

   Postconditions:

   returns the determinant of a.
*/

float vg_mat3x3_affine_det(const VG_MAT3X3_T *a)
{
   vcos_assert(vg_mat3x3_is_affine_or_nans(a));
   return (a->m[0][0] * a->m[1][1]) - (a->m[1][0] * a->m[0][1]);
}

/*
   Preconditions:

   -

   Postconditions:

   returns false iff a is not invertible (or very close to not being
   invertible).
*/

bool vg_mat3x3_is_invertible(const VG_MAT3X3_T *a)
{
   return absf_(vg_mat3x3_det(a)) >= EPS;
}

/*
   Preconditions:

   a must be affine (or have nans in the bad elements).

   Postconditions:

   returns false iff a is not invertible (or very close to not being
   invertible).
*/

bool vg_mat3x3_affine_is_invertible(const VG_MAT3X3_T *a)
{
   return absf_(vg_mat3x3_affine_det(a)) >= EPS;
}

/*
   Preconditions:

   a must be invertible, according to vg_mat3x3_is_invertible.

   Postconditions:

   a is inverted.
*/

void vg_mat3x3_invert(VG_MAT3X3_T *a)
{
   float oo_det;
   VG_MAT3X3_T b;

   vcos_assert(vg_mat3x3_is_invertible(a));

   oo_det = recip_(vg_mat3x3_det(a));

   b.m[0][0] = ((a->m[2][2] * a->m[1][1]) - (a->m[2][1] * a->m[1][2])) * oo_det;
   b.m[0][1] = ((a->m[0][2] * a->m[2][1]) - (a->m[0][1] * a->m[2][2])) * oo_det;
   b.m[0][2] = ((a->m[1][2] * a->m[0][1]) - (a->m[1][1] * a->m[0][2])) * oo_det;

   b.m[1][0] = ((a->m[2][0] * a->m[1][2]) - (a->m[2][2] * a->m[1][0])) * oo_det;
   b.m[1][1] = ((a->m[0][0] * a->m[2][2]) - (a->m[0][2] * a->m[2][0])) * oo_det;
   b.m[1][2] = ((a->m[1][0] * a->m[0][2]) - (a->m[1][2] * a->m[0][0])) * oo_det;

   b.m[2][0] = ((a->m[2][1] * a->m[1][0]) - (a->m[2][0] * a->m[1][1])) * oo_det;
   b.m[2][1] = ((a->m[0][1] * a->m[2][0]) - (a->m[0][0] * a->m[2][1])) * oo_det;
   b.m[2][2] = ((a->m[1][1] * a->m[0][0]) - (a->m[1][0] * a->m[0][1])) * oo_det;

   *a = b;
}

/*
   Preconditions:

   a must be affine (or have nans in the bad elements).
   a must be invertible, according to vg_mat3x3_affine_is_invertible.

   Postconditions:

   a is inverted.
*/

void vg_mat3x3_affine_invert(VG_MAT3X3_T *a)
{
   float oo_det;
   float m00, m01, m02, m10;

   vcos_assert(vg_mat3x3_affine_is_invertible(a));

   oo_det = recip_(vg_mat3x3_affine_det(a));
   m00 = a->m[0][0];
   m01 = a->m[0][1];
   m02 = a->m[0][2];
   m10 = a->m[1][0];

   a->m[0][0] = a->m[1][1] * oo_det;
   a->m[0][1] = -m01 * oo_det;
   a->m[0][2] = ((a->m[1][2] * m01) - (a->m[1][1] * m02)) * oo_det;

   a->m[1][0] = -m10 * oo_det;
   a->m[1][1] = m00 * oo_det;
   a->m[1][2] = ((m10 * m02) - (a->m[1][2] * m00)) * oo_det;

   a->m[2][0] = 0.0f;
   a->m[2][1] = 0.0f;
   a->m[2][2] = 1.0f;
}

/*
   Preconditions:

   -

   Postconditions:

   (x, y, -) is set to a * (x, y, 1).
*/

void vg_mat3x3_affine_transform(const VG_MAT3X3_T *a, float *x_io, float *y_io)
{
   float x = (a->m[0][0] * *x_io) + (a->m[0][1] * *y_io) + a->m[0][2];
   float y = (a->m[1][0] * *x_io) + (a->m[1][1] * *y_io) + a->m[1][2];
   *x_io = x;
   *y_io = y;
}

/*
   Preconditions:

   -

   Postconditions:

   (x, y, -) is set to a * (x, y, 0).
*/

void vg_mat3x3_affine_transform_t(const VG_MAT3X3_T *a, float *x_io, float *y_io)
{
   float x = (a->m[0][0] * *x_io) + (a->m[0][1] * *y_io);
   float y = (a->m[1][0] * *x_io) + (a->m[1][1] * *y_io);
   *x_io = x;
   *y_io = y;
}

/*
   the top-left 2x2 submatrix of a is decomposed into (R * S * Q), where R is a
   rotation matrix, S is a scale matrix, and Q is a rotation/flip matrix. note
   that this decomposition is not unique.

   Preconditions:

   r may be NULL.
   s0 may be NULL.
   s1 may be NULL.

   Postconditions:

   if r is non-NULL, it is set to the angle of R (so R = rotation_matrix(r)) in
   radians.
   if s0 is non-NULL, it is set to S[0][0].
   if s1 is non-NULL, it is set to S[1][1].
   so if we set both, S = scale_matrix(s0, s1).
*/

void vg_mat3x3_rsq(const VG_MAT3X3_T *a,
   float *r, float *s0, float *s1)
{
   /*
      a = R * S * Q (svd, R is rotation, S is scale, Q is rotation/flip)
      a^T = Q^T * S * R^T
      a * a^T = R * S * Q * Q^T * S * R^T = R * S^2 * R^T

      eigenvalues of a * a^T will give S^2
      eigenvectors of a * a^T will give R
   */

   /*
      ( b c ) = a * a^T
      ( d e )
   */

   float b = (a->m[0][0] * a->m[0][0]) + (a->m[0][1] * a->m[0][1]);
   float c = (a->m[0][0] * a->m[1][0]) + (a->m[0][1] * a->m[1][1]);
   /* d = c */
   float e = (a->m[1][0] * a->m[1][0]) + (a->m[1][1] * a->m[1][1]);

   float bpe = b + e;
   float bme = b - e;

   /*
      solve:

      bx + cy = sx
      dx + ey = sy

      cy * dx = (s - b)x * (s - e)y
      c^2 = (s - b) * (s - e)
      s^2 - (b + e)s + (be - c^2) = 0
      s = (b + e +/- sqrt((b + e)^2 - 4(be - c^2))) / 2
      s = (b + e +/- sqrt(b^2 + e^2 - 2be + 4c^2)) / 2
      s = (b + e +/- sqrt((b - e)^2 + 4c^2)) / 2
   */

   float t = sqrt_((bme * bme) + (4.0f * c * c));
   float v = (bpe + t) * 0.5f; /* first eigenvalue */
   if (s0) {
      *s0 = sqrt_(v);
   }
   if (s1) {
      *s1 = sqrt_(
         /* second eigenvalue */
         _maxf(bpe - t, 0.0f) * 0.5f);
   }

   /*
      angle of eigenvector corresponds to r
   */

   if (r) {
      /* first eigenvector is (c, v - b) / (v - e, c) */
      float x = (v - e) + c;
      float y = (v - b) + c;
      *r = ((absf_(x) < EPS) && (absf_(y) < EPS)) ? 0.0f : atan2_(y, x);
   }
}

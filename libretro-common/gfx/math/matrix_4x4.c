/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (matrix_4x4.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <string.h>
#include <math.h>

#include <gfx/math/matrix_4x4.h>
#include <gfx/math/vector_3.h>

void matrix_4x4_lookat(math_matrix_4x4 *out,
      vec3_t eye,
      vec3_t center,
      vec3_t up)
{
   vec3_t zaxis;   /* the "forward" vector */
   vec3_t xaxis;   /* the "right"   vector */
   vec3_t yaxis;   /* the "up"      vector */

   vec3_copy(&zaxis[0], center);
   vec3_subtract(&zaxis[0], eye);
   vec3_normalize(&zaxis[0]);

   vec3_cross(&xaxis[0], &zaxis[0], up);
   vec3_normalize(&xaxis[0]);
   vec3_cross(&yaxis[0], &xaxis[0], zaxis);

   MAT_ELEM_4X4(*out, 0, 0) = xaxis[0];
   MAT_ELEM_4X4(*out, 0, 1) = yaxis[0];
   MAT_ELEM_4X4(*out, 0, 2) = -zaxis[0];
   MAT_ELEM_4X4(*out, 0, 3) = 0.0;

   MAT_ELEM_4X4(*out, 1, 0) = xaxis[1];
   MAT_ELEM_4X4(*out, 1, 1) = yaxis[1];
   MAT_ELEM_4X4(*out, 1, 2) = -zaxis[1];
   MAT_ELEM_4X4(*out, 1, 3) = 0.0f;

   MAT_ELEM_4X4(*out, 2, 0) = xaxis[2];
   MAT_ELEM_4X4(*out, 2, 1) = yaxis[2];
   MAT_ELEM_4X4(*out, 2, 2) = -zaxis[2];
   MAT_ELEM_4X4(*out, 2, 3) = 0.0f;

   MAT_ELEM_4X4(*out, 3, 0) = -(xaxis[0] * eye[0] + xaxis[1] * eye[1] + xaxis[2] * eye[2]);
   MAT_ELEM_4X4(*out, 3, 1) = -(yaxis[0] * eye[0] + yaxis[1] * eye[1] + yaxis[2] * eye[2]);
   MAT_ELEM_4X4(*out, 3, 2) = -(zaxis[0] * eye[0] + zaxis[1] * eye[1] + zaxis[2] * eye[2]);
   MAT_ELEM_4X4(*out, 3, 3) = 1.f;
}

/*
 * Multiplies a with b, stores the result in out
 */
void matrix_4x4_multiply(
      math_matrix_4x4 *out,
      const math_matrix_4x4 *a,
      const math_matrix_4x4 *b)
{
   unsigned r, c, k;
   math_matrix_4x4 mat;

   if (!out || !a || !b)
      return;

   for (r = 0; r < 4; r++)
   {
      for (c = 0; c < 4; c++)
      {
         float dot = 0.0f;
         for (k = 0; k < 4; k++)
            dot += MAT_ELEM_4X4(*a, r, k) * MAT_ELEM_4X4(*b, k, c);
         MAT_ELEM_4X4(mat, r, c) = dot;
      }
   }

   *out = mat;
}

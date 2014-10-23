/* Copyright  (C) 2010-2014 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (matrix.c).
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

#include <gfx/math/matrix.h>
#include <string.h>
#include <math.h>

void matrix_identity(math_matrix *mat)
{
   unsigned i;
   memset(mat, 0, sizeof(*mat));
   for (i = 0; i < 4; i++)
      MAT_ELEM(*mat, i, i) = 1.0f;
}

void matrix_transpose(math_matrix *out, const math_matrix *in)
{
   unsigned i, j;
   math_matrix mat;
   for (i = 0; i < 4; i++)
      for (j = 0; j < 4; j++)
         MAT_ELEM(mat, j, i) = MAT_ELEM(*in, i, j);

   *out = mat;
}

void matrix_rotate_x(math_matrix *mat, float rad)
{
   float cosine = cosf(rad);
   float sine   = sinf(rad);

   matrix_identity(mat);

   MAT_ELEM(*mat, 1, 1) = cosine;
   MAT_ELEM(*mat, 2, 2) = cosine;
   MAT_ELEM(*mat, 1, 2) = -sine;
   MAT_ELEM(*mat, 2, 1) = sine;
}

void matrix_rotate_y(math_matrix *mat, float rad)
{
   float cosine = cosf(rad);
   float sine   = sinf(rad);

   matrix_identity(mat);

   MAT_ELEM(*mat, 0, 0) = cosine;
   MAT_ELEM(*mat, 2, 2) = cosine;
   MAT_ELEM(*mat, 0, 2) = -sine;
   MAT_ELEM(*mat, 2, 0) = sine;
}

void matrix_rotate_z(math_matrix *mat, float rad)
{
   float cosine = cosf(rad);
   float sine   = sinf(rad);

   matrix_identity(mat);

   MAT_ELEM(*mat, 0, 0) = cosine;
   MAT_ELEM(*mat, 1, 1) = cosine;
   MAT_ELEM(*mat, 0, 1) = -sine;
   MAT_ELEM(*mat, 1, 0) = sine;
}

void matrix_ortho(math_matrix *mat,
      float left, float right,
      float bottom, float top,
      float znear, float zfar)
{
   matrix_identity(mat);

   float tx = -(right + left) / (right - left);
   float ty = -(top + bottom) / (top - bottom);
   float tz = -(zfar + znear) / (zfar - znear);

   MAT_ELEM(*mat, 0, 0) =  2.0f / (right - left);
   MAT_ELEM(*mat, 1, 1) =  2.0f / (top - bottom);
   MAT_ELEM(*mat, 2, 2) = -2.0f / (zfar - znear);
   MAT_ELEM(*mat, 0, 3) = tx;
   MAT_ELEM(*mat, 1, 3) = ty;
   MAT_ELEM(*mat, 2, 3) = tz;
}

void matrix_scale(math_matrix *out, float x, float y,
      float z)
{
   memset(out, 0, sizeof(*out));
   MAT_ELEM(*out, 0, 0) = x;
   MAT_ELEM(*out, 1, 1) = y;
   MAT_ELEM(*out, 2, 2) = z;
   MAT_ELEM(*out, 3, 3) = 1.0f;
}

void matrix_translate(math_matrix *out, float x,
      float y, float z)
{
   matrix_identity(out);
   MAT_ELEM(*out, 0, 3) = x;
   MAT_ELEM(*out, 1, 3) = y;
   MAT_ELEM(*out, 2, 3) = z;
}

void matrix_projection(math_matrix *out, float znear,
      float zfar)
{
   memset(out, 0, sizeof(*out));
   MAT_ELEM(*out, 0, 0) = znear;
   MAT_ELEM(*out, 1, 1) = zfar;
   MAT_ELEM(*out, 2, 2) = (zfar + znear) / (zfar - znear);
   MAT_ELEM(*out, 2, 3) = -2.0f * zfar * znear / (zfar - znear);
   MAT_ELEM(*out, 3, 2) = -1.0f;
}

void matrix_multiply(math_matrix *out,
      const math_matrix *a, const math_matrix *b)
{
   unsigned r, c, k;
   math_matrix mat;

   for (r = 0; r < 4; r++)
   {
      for (c = 0; c < 4; c++)
      {
         float dot = 0.0f;
         for (k = 0; k < 4; k++)
            dot += MAT_ELEM(*a, r, k) * MAT_ELEM(*b, k, c);
         MAT_ELEM(mat, r, c) = dot;
      }
   }

   *out = mat;
}


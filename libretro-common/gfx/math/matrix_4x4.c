/* Copyright  (C) 2010-2015 The RetroArch team
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

#include <string.h>
#include <math.h>

#include <gfx/math/matrix_4x4.h>

/*
 * Sets mat to an identity matrix
 */
void matrix_4x4_identity(math_matrix_4x4 *mat)
{
   unsigned i;

   memset(mat, 0, sizeof(*mat));
   for (i = 0; i < 4; i++)
      MAT_ELEM_4X4(*mat, i, i) = 1.0f;
}

/*
 * Sets out to the transposed matrix of in
 */
void matrix_4x4_transpose(math_matrix_4x4 *out, const math_matrix_4x4 *in)
{
   unsigned i, j;
   math_matrix_4x4 mat;

   for (i = 0; i < 4; i++)
      for (j = 0; j < 4; j++)
         MAT_ELEM_4X4(mat, j, i) = MAT_ELEM_4X4(*in, i, j);

   *out = mat;
}

/*
 * Builds an X-axis rotation matrix
 */
void matrix_4x4_rotate_x(math_matrix_4x4 *mat, float rad)
{
   float cosine = cosf(rad);
   float sine   = sinf(rad);

   matrix_4x4_identity(mat);

   MAT_ELEM_4X4(*mat, 1, 1) = cosine;
   MAT_ELEM_4X4(*mat, 2, 2) = cosine;
   MAT_ELEM_4X4(*mat, 1, 2) = -sine;
   MAT_ELEM_4X4(*mat, 2, 1) = sine;
}

/*
 * Builds a rotation matrix using the 
 * rotation around the Y-axis.
 */
void matrix_4x4_rotate_y(math_matrix_4x4 *mat, float rad)
{
   float cosine = cosf(rad);
   float sine   = sinf(rad);

   matrix_4x4_identity(mat);

   MAT_ELEM_4X4(*mat, 0, 0) = cosine;
   MAT_ELEM_4X4(*mat, 2, 2) = cosine;
   MAT_ELEM_4X4(*mat, 0, 2) = -sine;
   MAT_ELEM_4X4(*mat, 2, 0) = sine;
}

/*
 * Builds a rotation matrix using the 
 * rotation around the Z-axis.
 */
void matrix_4x4_rotate_z(math_matrix_4x4 *mat, float rad)
{
   float cosine = cosf(rad);
   float sine   = sinf(rad);

   matrix_4x4_identity(mat);

   MAT_ELEM_4X4(*mat, 0, 0) = cosine;
   MAT_ELEM_4X4(*mat, 1, 1) = cosine;
   MAT_ELEM_4X4(*mat, 0, 1) = -sine;
   MAT_ELEM_4X4(*mat, 1, 0) = sine;
}

/*
 * Creates an orthographic projection matrix.
 */
void matrix_4x4_ortho(math_matrix_4x4 *mat,
      float left, float right,
      float bottom, float top,
      float znear, float zfar)
{
   float tx, ty, tz;

   matrix_4x4_identity(mat);

   tx = -(right + left) / (right - left);
   ty = -(top + bottom) / (top - bottom);
   tz = -(zfar + znear) / (zfar - znear);

   MAT_ELEM_4X4(*mat, 0, 0) =  2.0f / (right - left);
   MAT_ELEM_4X4(*mat, 1, 1) =  2.0f / (top - bottom);
   MAT_ELEM_4X4(*mat, 2, 2) = -2.0f / (zfar - znear);
   MAT_ELEM_4X4(*mat, 0, 3) = tx;
   MAT_ELEM_4X4(*mat, 1, 3) = ty;
   MAT_ELEM_4X4(*mat, 2, 3) = tz;
}

void matrix_4x4_scale(math_matrix_4x4 *out, float x, float y,
      float z)
{
   memset(out, 0, sizeof(*out));
   MAT_ELEM_4X4(*out, 0, 0) = x;
   MAT_ELEM_4X4(*out, 1, 1) = y;
   MAT_ELEM_4X4(*out, 2, 2) = z;
   MAT_ELEM_4X4(*out, 3, 3) = 1.0f;
}

/*
 * Builds a translation matrix. All other elements in 
 * the matrix will be set to zero except for the
 * diagonal which is set to 1.0
 */
void matrix_4x4_translate(math_matrix_4x4 *out, float x,
      float y, float z)
{
   matrix_4x4_identity(out);
   MAT_ELEM_4X4(*out, 0, 3) = x;
   MAT_ELEM_4X4(*out, 1, 3) = y;
   MAT_ELEM_4X4(*out, 2, 3) = z;
}

/*
 * Creates a perspective projection matrix.
 */
void matrix_4x4_projection(math_matrix_4x4 *out, float znear,
      float zfar)
{
   float delta_z = zfar - znear;

   memset(out, 0, sizeof(*out));
   MAT_ELEM_4X4(*out, 0, 0) = znear;
   MAT_ELEM_4X4(*out, 1, 1) = zfar;
   MAT_ELEM_4X4(*out, 2, 2) = (zfar + znear) / delta_z;
   MAT_ELEM_4X4(*out, 2, 3) = -2.0f * zfar * znear / delta_z;
   MAT_ELEM_4X4(*out, 3, 2) = -1.0f;
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

/* Copyright  (C) 2010-2016 The RetroArch team
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

void matrix_4x4_copy(math_matrix_4x4 *dst, const math_matrix_4x4 *src)
{
   unsigned i, j;

   for (i = 0; i < 4; i++)
      for (j = 0; j < 4; j++)
      MAT_ELEM_4X4(*dst, i, j) = MAT_ELEM_4X4(*src, i, j);
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
   float cosine             = cosf(rad);
   float sine               = sinf(rad);

   MAT_ELEM_4X4(*mat, 0, 0) = 1.0f;
   MAT_ELEM_4X4(*mat, 0, 1) = 0.0f;
   MAT_ELEM_4X4(*mat, 0, 2) = 0.0f;
   MAT_ELEM_4X4(*mat, 0, 3) = 0.0f;
   MAT_ELEM_4X4(*mat, 1, 0) = 0.0f;
   MAT_ELEM_4X4(*mat, 1, 1) = cosine;
   MAT_ELEM_4X4(*mat, 1, 2) = -sine;
   MAT_ELEM_4X4(*mat, 1, 3) = 0.0f;
   MAT_ELEM_4X4(*mat, 2, 0) = 0.0f;
   MAT_ELEM_4X4(*mat, 2, 1) = sine;
   MAT_ELEM_4X4(*mat, 2, 2) = cosine;
   MAT_ELEM_4X4(*mat, 2, 3) = 0.0f;
   MAT_ELEM_4X4(*mat, 3, 0) = 0.0f;
   MAT_ELEM_4X4(*mat, 3, 1) = 0.0f;
   MAT_ELEM_4X4(*mat, 3, 2) = 0.0f;
   MAT_ELEM_4X4(*mat, 3, 3) = 1.0f;
}

/*
 * Builds a rotation matrix using the 
 * rotation around the Y-axis.
 */
void matrix_4x4_rotate_y(math_matrix_4x4 *mat, float rad)
{
   float cosine             = cosf(rad);
   float sine               = sinf(rad);

   MAT_ELEM_4X4(*mat, 0, 0) = cosine;
   MAT_ELEM_4X4(*mat, 0, 1) = 0.0f;
   MAT_ELEM_4X4(*mat, 0, 2) = -sine;
   MAT_ELEM_4X4(*mat, 0, 3) = 0.0f;

   MAT_ELEM_4X4(*mat, 1, 0) = 0.0f;
   MAT_ELEM_4X4(*mat, 1, 1) = 1.0f;
   MAT_ELEM_4X4(*mat, 1, 2) = 0.0f;
   MAT_ELEM_4X4(*mat, 1, 3) = 0.0f;

   MAT_ELEM_4X4(*mat, 2, 0) = sine;
   MAT_ELEM_4X4(*mat, 2, 1) = 0.0f;
   MAT_ELEM_4X4(*mat, 2, 2) = cosine;
   MAT_ELEM_4X4(*mat, 2, 3) = 0.0f;

   MAT_ELEM_4X4(*mat, 3, 0) = 0.0f;
   MAT_ELEM_4X4(*mat, 3, 1) = 0.0f;
   MAT_ELEM_4X4(*mat, 3, 2) = 0.0f;
   MAT_ELEM_4X4(*mat, 3, 3) = 1.0f;
}

/*
 * Builds a rotation matrix using the 
 * rotation around the Z-axis.
 */
void matrix_4x4_rotate_z(math_matrix_4x4 *mat, float rad)
{
   float cosine             = cosf(rad);
   float sine               = sinf(rad);

   MAT_ELEM_4X4(*mat, 0, 0) = cosine;
   MAT_ELEM_4X4(*mat, 0, 1) = -sine;
   MAT_ELEM_4X4(*mat, 0, 2) = 0.0f;
   MAT_ELEM_4X4(*mat, 0, 3) = 0.0f;
   MAT_ELEM_4X4(*mat, 1, 0) = sine;
   MAT_ELEM_4X4(*mat, 1, 1) = cosine;
   MAT_ELEM_4X4(*mat, 1, 2) = 0.0f;
   MAT_ELEM_4X4(*mat, 1, 3) = 0.0f;
   MAT_ELEM_4X4(*mat, 2, 0) = 0.0f;
   MAT_ELEM_4X4(*mat, 2, 1) = 0.0f;
   MAT_ELEM_4X4(*mat, 2, 2) = 1.0f;
   MAT_ELEM_4X4(*mat, 2, 3) = 0.0f;
   MAT_ELEM_4X4(*mat, 3, 0) = 0.0f;
   MAT_ELEM_4X4(*mat, 3, 1) = 0.0f;
   MAT_ELEM_4X4(*mat, 3, 2) = 0.0f;
   MAT_ELEM_4X4(*mat, 3, 3) = 1.0f;
}

/*
 * Creates an orthographic projection matrix.
 */
void matrix_4x4_ortho(math_matrix_4x4 *mat,
      float left, float right,
      float bottom, float top,
      float znear, float zfar)
{
   float rl                 = right - left;
   float tb                 = top   - bottom;
   float fn                 = zfar  - znear;

   MAT_ELEM_4X4(*mat, 0, 0) =  2.0f / rl;
   MAT_ELEM_4X4(*mat, 0, 1) =  0.0f;
   MAT_ELEM_4X4(*mat, 0, 2) =  0.0f;
   MAT_ELEM_4X4(*mat, 0, 3) = -(left + right)  / rl;
   MAT_ELEM_4X4(*mat, 1, 0) =  0.0f;
   MAT_ELEM_4X4(*mat, 1, 1) =  2.0f / tb;
   MAT_ELEM_4X4(*mat, 1, 2) =  0.0f;
   MAT_ELEM_4X4(*mat, 1, 3) = -(top  + bottom) / tb;
   MAT_ELEM_4X4(*mat, 2, 0) =  0.0f;
   MAT_ELEM_4X4(*mat, 2, 1) =  0.0f;
   MAT_ELEM_4X4(*mat, 2, 2) = -2.0f / fn;
   MAT_ELEM_4X4(*mat, 2, 3) = -(zfar + znear)  / fn;
   MAT_ELEM_4X4(*mat, 3, 0) =  0.0f;
   MAT_ELEM_4X4(*mat, 3, 1) =  0.0f;
   MAT_ELEM_4X4(*mat, 3, 2) =  0.0f;
   MAT_ELEM_4X4(*mat, 3, 3) =  1.0f;
}

void matrix_4x4_scale(math_matrix_4x4 *out, float x, float y,
      float z)
{
   MAT_ELEM_4X4(*out, 0, 0) = x;
   MAT_ELEM_4X4(*out, 0, 1) = 0.0f;
   MAT_ELEM_4X4(*out, 0, 2) = 0.0f;
   MAT_ELEM_4X4(*out, 0, 3) = 0.0f;
   MAT_ELEM_4X4(*out, 1, 0) = 0.0f;
   MAT_ELEM_4X4(*out, 1, 1) = y;
   MAT_ELEM_4X4(*out, 1, 2) = 0.0f;
   MAT_ELEM_4X4(*out, 1, 3) = 0.0f;
   MAT_ELEM_4X4(*out, 2, 0) = 0.0f;
   MAT_ELEM_4X4(*out, 2, 1) = 0.0f;
   MAT_ELEM_4X4(*out, 2, 2) = z;
   MAT_ELEM_4X4(*out, 2, 3) = 0.0f;
   MAT_ELEM_4X4(*out, 3, 0) = 0.0f;
   MAT_ELEM_4X4(*out, 3, 1) = 0.0f;
   MAT_ELEM_4X4(*out, 3, 2) = 0.0f;
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
   MAT_ELEM_4X4(*out, 0, 0) = 1.0f;
   MAT_ELEM_4X4(*out, 0, 1) = 0.0f;
   MAT_ELEM_4X4(*out, 0, 2) = 0.0f;
   MAT_ELEM_4X4(*out, 0, 3) = x;
   MAT_ELEM_4X4(*out, 1, 0) = 0.0f;
   MAT_ELEM_4X4(*out, 1, 1) = 1.0f;
   MAT_ELEM_4X4(*out, 1, 2) = 1.0f;
   MAT_ELEM_4X4(*out, 1, 3) = y;
   MAT_ELEM_4X4(*out, 2, 0) = 0.0f;
   MAT_ELEM_4X4(*out, 2, 1) = 0.0f;
   MAT_ELEM_4X4(*out, 2, 2) = 1.0f;
   MAT_ELEM_4X4(*out, 2, 3) = z;
   MAT_ELEM_4X4(*out, 3, 0) = 0.0f;
   MAT_ELEM_4X4(*out, 3, 1) = 0.0f;
   MAT_ELEM_4X4(*out, 3, 2) = 0.0f;
   MAT_ELEM_4X4(*out, 3, 3) = 1.0f;
}

/*
 * Creates a perspective projection matrix.
 */
void matrix_4x4_projection(math_matrix_4x4 *out, 
      float y_fov,
      float aspect,
      float znear,
      float zfar)
{
   float const a            = 1.f / tan(y_fov / 2.f);
   float delta_z            = zfar - znear;

   MAT_ELEM_4X4(*out, 0, 0) = a / aspect;
   MAT_ELEM_4X4(*out, 0, 1) = 0.0f;
   MAT_ELEM_4X4(*out, 0, 2) = 0.0f;
   MAT_ELEM_4X4(*out, 0, 3) = 0.0f;
   MAT_ELEM_4X4(*out, 1, 0) = 0.0f;
   MAT_ELEM_4X4(*out, 1, 1) = a;
   MAT_ELEM_4X4(*out, 1, 2) = 0.0f;
   MAT_ELEM_4X4(*out, 1, 3) = 0.0f;
   MAT_ELEM_4X4(*out, 2, 0) = 0.0f;
   MAT_ELEM_4X4(*out, 2, 1) = 0.0f;
   MAT_ELEM_4X4(*out, 2, 2) = -((zfar + znear) / delta_z);
   MAT_ELEM_4X4(*out, 2, 3) = -((2.f * zfar * znear) / delta_z);
   MAT_ELEM_4X4(*out, 3, 0) = 0.0f;
   MAT_ELEM_4X4(*out, 3, 1) = 0.0f;
   MAT_ELEM_4X4(*out, 3, 2) = -1.f;
   MAT_ELEM_4X4(*out, 3, 3) = 1.0f;
}

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

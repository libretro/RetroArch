/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (matrix_3x3.h).
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

#ifndef __LIBRETRO_SDK_GFX_MATH_MATRIX_3X3_H__
#define __LIBRETRO_SDK_GFX_MATH_MATRIX_3X3_H__

#include <boolean.h>
#include <math.h>
#include <string.h>

#include <retro_common_api.h>
#include <retro_inline.h>

RETRO_BEGIN_DECLS

typedef struct math_matrix_3x3
{
   float data[9];
} math_matrix_3x3;

#define MAT_ELEM_3X3(mat, r, c) ((mat).data[3 * (r) + (c)])

#define matrix_3x3_init(mat, n11, n12, n13, n21, n22, n23, n31, n32, n33) \
   MAT_ELEM_3X3(mat, 0, 0) = n11; \
   MAT_ELEM_3X3(mat, 0, 1) = n12; \
   MAT_ELEM_3X3(mat, 0, 2) = n13; \
   MAT_ELEM_3X3(mat, 1, 0) = n21; \
   MAT_ELEM_3X3(mat, 1, 1) = n22; \
   MAT_ELEM_3X3(mat, 1, 2) = n23; \
   MAT_ELEM_3X3(mat, 2, 0) = n31; \
   MAT_ELEM_3X3(mat, 2, 1) = n32; \
   MAT_ELEM_3X3(mat, 2, 2) = n33

#define matrix_3x3_identity(mat) \
   MAT_ELEM_3X3(mat, 0, 0) = 1.0f; \
   MAT_ELEM_3X3(mat, 0, 1) = 0; \
   MAT_ELEM_3X3(mat, 0, 2) = 0; \
   MAT_ELEM_3X3(mat, 1, 0) = 0; \
   MAT_ELEM_3X3(mat, 1, 1) = 1.0f; \
   MAT_ELEM_3X3(mat, 1, 2) = 0; \
   MAT_ELEM_3X3(mat, 2, 0) = 0; \
   MAT_ELEM_3X3(mat, 2, 1) = 0; \
   MAT_ELEM_3X3(mat, 2, 2) = 1.0f

#define matrix_3x3_divide_scalar(mat, s) \
   MAT_ELEM_3X3(mat, 0, 0) /= s; \
   MAT_ELEM_3X3(mat, 0, 1) /= s; \
   MAT_ELEM_3X3(mat, 0, 2) /= s; \
   MAT_ELEM_3X3(mat, 1, 0) /= s; \
   MAT_ELEM_3X3(mat, 1, 1) /= s; \
   MAT_ELEM_3X3(mat, 1, 2) /= s; \
   MAT_ELEM_3X3(mat, 2, 0) /= s; \
   MAT_ELEM_3X3(mat, 2, 1) /= s; \
   MAT_ELEM_3X3(mat, 2, 2) /= s

#define matrix_3x3_transpose(mat, in) \
   MAT_ELEM_3X3(mat, 0, 0) = MAT_ELEM_3X3(in, 0, 0); \
   MAT_ELEM_3X3(mat, 1, 0) = MAT_ELEM_3X3(in, 0, 1); \
   MAT_ELEM_3X3(mat, 2, 0) = MAT_ELEM_3X3(in, 0, 2); \
   MAT_ELEM_3X3(mat, 0, 1) = MAT_ELEM_3X3(in, 1, 0); \
   MAT_ELEM_3X3(mat, 1, 1) = MAT_ELEM_3X3(in, 1, 1); \
   MAT_ELEM_3X3(mat, 2, 1) = MAT_ELEM_3X3(in, 1, 2); \
   MAT_ELEM_3X3(mat, 0, 2) = MAT_ELEM_3X3(in, 2, 0); \
   MAT_ELEM_3X3(mat, 1, 2) = MAT_ELEM_3X3(in, 2, 1); \
   MAT_ELEM_3X3(mat, 2, 2) = MAT_ELEM_3X3(in, 2, 2)

#define matrix_3x3_multiply(out, a, b) \
   MAT_ELEM_3X3(out, 0, 0) =  \
      MAT_ELEM_3X3(a, 0, 0) * MAT_ELEM_3X3(b, 0, 0) + \
      MAT_ELEM_3X3(a, 0, 1) * MAT_ELEM_3X3(b, 1, 0) + \
      MAT_ELEM_3X3(a, 0, 2) * MAT_ELEM_3X3(b, 2, 0); \
   MAT_ELEM_3X3(out, 0, 1) = \
      MAT_ELEM_3X3(a, 0, 0) * MAT_ELEM_3X3(b, 0, 1) + \
      MAT_ELEM_3X3(a, 0, 1) * MAT_ELEM_3X3(b, 1, 1) + \
      MAT_ELEM_3X3(a, 0, 2) * MAT_ELEM_3X3(b, 2, 1); \
   MAT_ELEM_3X3(out, 0, 2) = \
      MAT_ELEM_3X3(a, 0, 0) * MAT_ELEM_3X3(b, 0, 2) + \
      MAT_ELEM_3X3(a, 0, 1) * MAT_ELEM_3X3(b, 1, 2) + \
      MAT_ELEM_3X3(a, 0, 2) * MAT_ELEM_3X3(b, 2, 2); \
   MAT_ELEM_3X3(out, 1, 0) = \
      MAT_ELEM_3X3(a, 1, 0) * MAT_ELEM_3X3(b, 0, 0) + \
      MAT_ELEM_3X3(a, 1, 1) * MAT_ELEM_3X3(b, 1, 0) + \
      MAT_ELEM_3X3(a, 1, 2) * MAT_ELEM_3X3(b, 2, 0); \
   MAT_ELEM_3X3(out, 1, 1) =  \
      MAT_ELEM_3X3(a, 1, 0) * MAT_ELEM_3X3(b, 0, 1) + \
      MAT_ELEM_3X3(a, 1, 1) * MAT_ELEM_3X3(b, 1, 1) + \
      MAT_ELEM_3X3(a, 1, 2) * MAT_ELEM_3X3(b, 2, 1); \
   MAT_ELEM_3X3(out, 1, 2) = \
      MAT_ELEM_3X3(a, 1, 0) * MAT_ELEM_3X3(b, 0, 2) + \
      MAT_ELEM_3X3(a, 1, 1) * MAT_ELEM_3X3(b, 1, 2) + \
      MAT_ELEM_3X3(a, 1, 2) * MAT_ELEM_3X3(b, 2, 2); \
   MAT_ELEM_3X3(out, 2, 0) =  \
      MAT_ELEM_3X3(a, 2, 0) * MAT_ELEM_3X3(b, 0, 0) + \
      MAT_ELEM_3X3(a, 2, 1) * MAT_ELEM_3X3(b, 1, 0) + \
      MAT_ELEM_3X3(a, 2, 2) * MAT_ELEM_3X3(b, 2, 0); \
   MAT_ELEM_3X3(out, 2, 1) = \
      MAT_ELEM_3X3(a, 2, 0) * MAT_ELEM_3X3(b, 0, 1) + \
      MAT_ELEM_3X3(a, 2, 1) * MAT_ELEM_3X3(b, 1, 1) + \
      MAT_ELEM_3X3(a, 2, 2) * MAT_ELEM_3X3(b, 2, 1); \
   MAT_ELEM_3X3(out, 2, 2) =  \
      MAT_ELEM_3X3(a, 2, 0) * MAT_ELEM_3X3(b, 0, 2) + \
      MAT_ELEM_3X3(a, 2, 1) * MAT_ELEM_3X3(b, 1, 2) + \
      MAT_ELEM_3X3(a, 2, 2) * MAT_ELEM_3X3(b, 2, 2)

#define matrix_3x3_determinant(mat) (MAT_ELEM_3X3(mat, 0, 0) * (MAT_ELEM_3X3(mat, 1, 1) * MAT_ELEM_3X3(mat, 2, 2) - MAT_ELEM_3X3(mat, 1, 2) * MAT_ELEM_3X3(mat, 2, 1)) - MAT_ELEM_3X3(mat, 0, 1) * (MAT_ELEM_3X3(mat, 1, 0) * MAT_ELEM_3X3(mat, 2, 2) - MAT_ELEM_3X3(mat, 1, 2) * MAT_ELEM_3X3(mat, 2, 0)) + MAT_ELEM_3X3(mat, 0, 2) * (MAT_ELEM_3X3(mat, 1, 0) * MAT_ELEM_3X3(mat, 2, 1) - MAT_ELEM_3X3(mat, 1, 1) * MAT_ELEM_3X3(mat, 2, 0)))

#define matrix_3x3_adjoint(mat) \
   MAT_ELEM_3X3(mat, 0, 0) =  (MAT_ELEM_3X3(mat, 1, 1) * MAT_ELEM_3X3(mat, 2, 2) - MAT_ELEM_3X3(mat, 1, 2) * MAT_ELEM_3X3(mat, 2, 1)); \
   MAT_ELEM_3X3(mat, 0, 1) = -(MAT_ELEM_3X3(mat, 0, 1) * MAT_ELEM_3X3(mat, 2, 2) - MAT_ELEM_3X3(mat, 0, 2) * MAT_ELEM_3X3(mat, 2, 1)); \
   MAT_ELEM_3X3(mat, 0, 2) =  (MAT_ELEM_3X3(mat, 0, 1) * MAT_ELEM_3X3(mat, 1, 1) - MAT_ELEM_3X3(mat, 0, 2) * MAT_ELEM_3X3(mat, 1, 1)); \
   MAT_ELEM_3X3(mat, 1, 0) = -(MAT_ELEM_3X3(mat, 1, 0) * MAT_ELEM_3X3(mat, 2, 2) - MAT_ELEM_3X3(mat, 1, 2) * MAT_ELEM_3X3(mat, 2, 0)); \
   MAT_ELEM_3X3(mat, 1, 1) =  (MAT_ELEM_3X3(mat, 0, 0) * MAT_ELEM_3X3(mat, 2, 2) - MAT_ELEM_3X3(mat, 0, 2) * MAT_ELEM_3X3(mat, 2, 0)); \
   MAT_ELEM_3X3(mat, 1, 2) = -(MAT_ELEM_3X3(mat, 0, 0) * MAT_ELEM_3X3(mat, 1, 2) - MAT_ELEM_3X3(mat, 0, 2) * MAT_ELEM_3X3(mat, 1, 0)); \
   MAT_ELEM_3X3(mat, 2, 0) =  (MAT_ELEM_3X3(mat, 1, 0) * MAT_ELEM_3X3(mat, 2, 1) - MAT_ELEM_3X3(mat, 1, 1) * MAT_ELEM_3X3(mat, 2, 0)); \
   MAT_ELEM_3X3(mat, 2, 1) = -(MAT_ELEM_3X3(mat, 0, 0) * MAT_ELEM_3X3(mat, 2, 1) - MAT_ELEM_3X3(mat, 0, 1) * MAT_ELEM_3X3(mat, 2, 0)); \
   MAT_ELEM_3X3(mat, 2, 2) =  (MAT_ELEM_3X3(mat, 0, 0) * MAT_ELEM_3X3(mat, 1, 1) - MAT_ELEM_3X3(mat, 0, 1) * MAT_ELEM_3X3(mat, 1, 0))

#define FLOATS_ARE_EQUAL(x, y)  (fabs(x - y) <= 0.00001f * ((x) > (y) ? (y) : (x)))
#define FLOAT_IS_ZERO(x)        (FLOATS_ARE_EQUAL((x) + 1, 1))

static INLINE bool matrix_3x3_invert(math_matrix_3x3 *mat)
{
   float det = matrix_3x3_determinant(*mat);

   if (FLOAT_IS_ZERO(det))
      return false;

   matrix_3x3_adjoint(*mat);
   matrix_3x3_divide_scalar(*mat, det);

   return true;
}

static INLINE bool matrix_3x3_square_to_quad(
      const float dx0, const float dy0,
      const float dx1, const float dy1,
      const float dx3, const float dy3,
      const float dx2, const float dy2,
      math_matrix_3x3 *mat)
{
   float a, b, d, e;
   float ax  = dx0 - dx1 + dx2 - dx3;
   float ay  = dy0 - dy1 + dy2 - dy3;
   float c   = dx0;
   float f   = dy0;
   float g   = 0;
   float h   = 0;

   if (FLOAT_IS_ZERO(ax) && FLOAT_IS_ZERO(ay))
   {
      /* affine case */
      a = dx1 - dx0;
      b = dx2 - dx1;
      d = dy1 - dy0;
      e = dy2 - dy1;
   }
   else
   {
      float ax1 = dx1 - dx2;
      float ax2 = dx3 - dx2;
      float ay1 = dy1 - dy2;
      float ay2 = dy3 - dy2;

      /* determinants */
      float gtop    =  ax  * ay2 - ax2 * ay;
      float htop    =  ax1 * ay  - ax  * ay1;
      float bottom  =  ax1 * ay2 - ax2 * ay1;

      if (!bottom)
         return false;

      g = gtop / bottom;
      h = htop / bottom;

      a = dx1 - dx0 + g * dx1;
      b = dx3 - dx0 + h * dx3;
      d = dy1 - dy0 + g * dy1;
      e = dy3 - dy0 + h * dy3;
   }

   matrix_3x3_init(*mat,
         a, d, g,
         b, e, h,
         c, f, 1.f);

   return true;
}

static INLINE bool matrix_3x3_quad_to_square(
      const float sx0, const float sy0,
      const float sx1, const float sy1,
      const float sx2, const float sy2,
      const float sx3, const float sy3,
      math_matrix_3x3 *mat)
{
   return matrix_3x3_square_to_quad(sx0, sy0, sx1, sy1,
         sx2, sy2, sx3, sy3,
         mat) ? matrix_3x3_invert(mat) : false;
}

static INLINE bool matrix_3x3_quad_to_quad(
      const float dx0, const float dy0,
      const float dx1, const float dy1,
      const float dx2, const float dy2,
      const float dx3, const float dy3,
      const float sx0, const float sy0,
      const float sx1, const float sy1,
      const float sx2, const float sy2,
      const float sx3, const float sy3,
      math_matrix_3x3 *mat)
{
   math_matrix_3x3 square_to_quad;

   if (matrix_3x3_square_to_quad(dx0, dy0, dx1, dy1,
            dx2, dy2, dx3, dy3,
            &square_to_quad))
   {
      math_matrix_3x3 quad_to_square;
      if (matrix_3x3_quad_to_square(sx0, sy0, sx1, sy1,
               sx2, sy2, sx3, sy3,
               &quad_to_square))
      {
         matrix_3x3_multiply(*mat, quad_to_square, square_to_quad);

         return true;
      }
   }

   return false;
}

RETRO_END_DECLS

#endif

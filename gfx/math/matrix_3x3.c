/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2012 - Michael Lelli
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "matrix_3x3.h"
#include <math.h>
#include <string.h>

#define floatsEqual(x, y) (fabs(x - y) <= 0.00001f * ((x) > (y) ? (y) : (x)))
#define floatIsZero(x) (floatsEqual((x) + 1, 1))

void matrix_3x3_identity(math_matrix_3x3 *mat)
{
   unsigned i;
   memset(mat, 0, sizeof(*mat));
   for (i = 0; i < 3; i++)
      MAT_ELEM_3X3(*mat, i, i) = 1.0f;
}

void matrix_3x3_inits(math_matrix_3x3 *mat,
                      const float n11, const float n12, const float n13,
                      const float n21, const float n22, const float n23,
                      const float n31, const float n32, const float n33)
{
   MAT_ELEM_3X3(*mat, 0, 0) = n11;
   MAT_ELEM_3X3(*mat, 0, 1) = n12;
   MAT_ELEM_3X3(*mat, 0, 2) = n13;
   MAT_ELEM_3X3(*mat, 1, 0) = n21;
   MAT_ELEM_3X3(*mat, 1, 1) = n22;
   MAT_ELEM_3X3(*mat, 1, 2) = n23;
   MAT_ELEM_3X3(*mat, 2, 0) = n31;
   MAT_ELEM_3X3(*mat, 2, 1) = n32;
   MAT_ELEM_3X3(*mat, 2, 2) = n33;
}

void matrix_3x3_transpose(math_matrix_3x3 *out, const math_matrix_3x3 *in)
{
   unsigned i, j;
   math_matrix_3x3 mat;
   for (i = 0; i < 3; i++)
      for (j = 0; j < 3; j++)
         MAT_ELEM_3X3(mat, j, i) = MAT_ELEM_3X3(*in, i, j);

   *out = mat;
}

void matrix_3x3_multiply(math_matrix_3x3 *out,
      const math_matrix_3x3 *a, const math_matrix_3x3 *b)
{
   unsigned r, c, k;
   math_matrix_3x3 mat;
   for (r = 0; r < 3; r++)
   {
      for (c = 0; c < 3; c++)
      {
         float dot = 0.0f;
         for (k = 0; k < 3; k++)
            dot += MAT_ELEM_3X3(*a, r, k) * MAT_ELEM_3X3(*b, k, c);
         MAT_ELEM_3X3(mat, r, c) = dot;
      }
   }

   *out = mat;
}

void matrix_3x3_divide_scalar(math_matrix_3x3 *mat, const float s)
{
   unsigned i, j;
   for (i = 0; i < 3; i++)
      for (j = 0; j < 3; j++)
         MAT_ELEM_3X3(*mat, i, j) /= s;
}

float matrix_3x3_determinant(const math_matrix_3x3 *mat)
{
   float det = MAT_ELEM_3X3(*mat, 0, 0) * (MAT_ELEM_3X3(*mat, 1, 1) * MAT_ELEM_3X3(*mat, 2, 2) - MAT_ELEM_3X3(*mat, 1, 2) * MAT_ELEM_3X3(*mat, 2, 1));
   det      -= MAT_ELEM_3X3(*mat, 0, 1) * (MAT_ELEM_3X3(*mat, 1, 0) * MAT_ELEM_3X3(*mat, 2, 2) - MAT_ELEM_3X3(*mat, 1, 2) * MAT_ELEM_3X3(*mat, 2, 0));
   det      += MAT_ELEM_3X3(*mat, 0, 2) * (MAT_ELEM_3X3(*mat, 1, 0) * MAT_ELEM_3X3(*mat, 2, 1) - MAT_ELEM_3X3(*mat, 1, 1) * MAT_ELEM_3X3(*mat, 2, 0));

   return det;
}

void matrix_3x3_adjoint(math_matrix_3x3 *mat)
{
   math_matrix_3x3 out;

   MAT_ELEM_3X3(out, 0, 0) =  (MAT_ELEM_3X3(*mat, 1, 1) * MAT_ELEM_3X3(*mat, 2, 2) - MAT_ELEM_3X3(*mat, 1, 2) * MAT_ELEM_3X3(*mat, 2, 1));
   MAT_ELEM_3X3(out, 0, 1) = -(MAT_ELEM_3X3(*mat, 0, 1) * MAT_ELEM_3X3(*mat, 2, 2) - MAT_ELEM_3X3(*mat, 0, 2) * MAT_ELEM_3X3(*mat, 2, 1));
   MAT_ELEM_3X3(out, 0, 2) =  (MAT_ELEM_3X3(*mat, 0, 1) * MAT_ELEM_3X3(*mat, 1, 1) - MAT_ELEM_3X3(*mat, 0, 2) * MAT_ELEM_3X3(*mat, 1, 1));
   MAT_ELEM_3X3(out, 1, 0) = -(MAT_ELEM_3X3(*mat, 1, 0) * MAT_ELEM_3X3(*mat, 2, 2) - MAT_ELEM_3X3(*mat, 1, 2) * MAT_ELEM_3X3(*mat, 2, 0));
   MAT_ELEM_3X3(out, 1, 1) =  (MAT_ELEM_3X3(*mat, 0, 0) * MAT_ELEM_3X3(*mat, 2, 2) - MAT_ELEM_3X3(*mat, 0, 2) * MAT_ELEM_3X3(*mat, 2, 0));
   MAT_ELEM_3X3(out, 1, 2) = -(MAT_ELEM_3X3(*mat, 0, 0) * MAT_ELEM_3X3(*mat, 1, 2) - MAT_ELEM_3X3(*mat, 0, 2) * MAT_ELEM_3X3(*mat, 1, 0));
   MAT_ELEM_3X3(out, 2, 0) =  (MAT_ELEM_3X3(*mat, 1, 0) * MAT_ELEM_3X3(*mat, 2, 1) - MAT_ELEM_3X3(*mat, 1, 1) * MAT_ELEM_3X3(*mat, 2, 0));
   MAT_ELEM_3X3(out, 2, 1) = -(MAT_ELEM_3X3(*mat, 0, 0) * MAT_ELEM_3X3(*mat, 2, 1) - MAT_ELEM_3X3(*mat, 0, 1) * MAT_ELEM_3X3(*mat, 2, 0));
   MAT_ELEM_3X3(out, 2, 2) =  (MAT_ELEM_3X3(*mat, 0, 0) * MAT_ELEM_3X3(*mat, 1, 1) - MAT_ELEM_3X3(*mat, 0, 1) * MAT_ELEM_3X3(*mat, 1, 0));

   *mat = out;
}

bool matrix_3x3_invert(math_matrix_3x3 *mat)
{
   float det = matrix_3x3_determinant(mat);

   if (floatIsZero(det))
      return false;

   matrix_3x3_adjoint(mat);
   matrix_3x3_divide_scalar(mat, det);
   return true;
}

/**************************************************************************
 *
 * the following code is Copyright 2009 VMware, Inc.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

bool matrix_3x3_square_to_quad(const float dx0, const float dy0,
                               const float dx1, const float dy1,
                               const float dx3, const float dy3,
                               const float dx2, const float dy2,
                               math_matrix_3x3 *mat)
{
   float ax  = dx0 - dx1 + dx2 - dx3;
   float ay  = dy0 - dy1 + dy2 - dy3;

   if (floatIsZero(ax) && floatIsZero(ay)) {
      /* affine case */
      matrix_3x3_inits(mat,
                       dx1 - dx0, dy1 - dy0,  0,
                       dx2 - dx1, dy2 - dy1,  0,
                       dx0,       dy0,        1);
   } else {
      float a, b, c, d, e, f, g, h;
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
      c = dx0;
      d = dy1 - dy0 + g * dy1;
      e = dy3 - dy0 + h * dy3;
      f = dy0;

      matrix_3x3_inits(mat,
                       a, d, g,
                       b, e, h,
                       c, f, 1.f);
   }

   return true;
}

bool matrix_3x3_quad_to_square(const float sx0, const float sy0,
                               const float sx1, const float sy1,
                               const float sx2, const float sy2,
                               const float sx3, const float sy3,
                               math_matrix_3x3 *mat)
{
   if (!matrix_3x3_square_to_quad(sx0, sy0, sx1, sy1,
                                  sx2, sy2, sx3, sy3,
                                  mat))
      return false;

   return matrix_3x3_invert(mat);
}

bool matrix_3x3_quad_to_quad(const float dx0, const float dy0,
                             const float dx1, const float dy1,
                             const float dx2, const float dy2,
                             const float dx3, const float dy3,
                             const float sx0, const float sy0,
                             const float sx1, const float sy1,
                             const float sx2, const float sy2,
                             const float sx3, const float sy3,
                             math_matrix_3x3 *mat)
{
   math_matrix_3x3 quad_to_square, square_to_quad;

   if (!matrix_3x3_square_to_quad(dx0, dy0, dx1, dy1,
                                  dx2, dy2, dx3, dy3,
                                  &square_to_quad))
      return false;

   if (!matrix_3x3_quad_to_square(sx0, sy0, sx1, sy1,
                                  sx2, sy2, sx3, sy3,
                                  &quad_to_square))
      return false;

   matrix_3x3_multiply(mat, &quad_to_square, &square_to_quad);

   return true;
}

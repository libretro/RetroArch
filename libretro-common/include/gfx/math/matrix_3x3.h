/* Copyright  (C) 2010-2016 The RetroArch team
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

typedef struct math_matrix_3x3
{
   float data[9];
} math_matrix_3x3;

#define MAT_ELEM_3X3(mat, r, c) ((mat).data[3 * (r) + (c)])

void matrix_3x3_inits(math_matrix_3x3 *mat,
                      const float n11, const float n12, const float n13,
                      const float n21, const float n22, const float n23,
                      const float n31, const float n32, const float n33);
void matrix_3x3_identity(math_matrix_3x3 *mat);
void matrix_3x3_transpose(math_matrix_3x3 *out, const math_matrix_3x3 *in);

void matrix_3x3_multiply(math_matrix_3x3 *out,
      const math_matrix_3x3 *a, const math_matrix_3x3 *b);
void matrix_3x3_divide_scalar(math_matrix_3x3 *mat, float s);
float matrix_3x3_determinant(const math_matrix_3x3 *mat);
void matrix_3x3_adjoint(math_matrix_3x3 *mat);
bool matrix_3x3_invert(math_matrix_3x3 *mat);

bool matrix_3x3_square_to_quad(const float dx0, const float dy0,
                               const float dx1, const float dy1,
                               const float dx3, const float dy3,
                               const float dx2, const float dy2,
                               math_matrix_3x3 *mat);
bool matrix_3x3_quad_to_square(const float sx0, const float sy0,
                               const float sx1, const float sy1,
                               const float sx2, const float sy2,
                               const float sx3, const float sy3,
                               math_matrix_3x3 *mat);
bool matrix_3x3_quad_to_quad(const float dx0, const float dy0,
                             const float dx1, const float dy1,
                             const float dx2, const float dy2,
                             const float dx3, const float dy3,
                             const float sx0, const float sy0,
                             const float sx1, const float sy1,
                             const float sx2, const float sy2,
                             const float sx3, const float sy3,
                             math_matrix_3x3 *mat);

#endif

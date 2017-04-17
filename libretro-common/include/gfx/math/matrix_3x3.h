/* Copyright  (C) 2010-2017 The RetroArch team
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

#include <retro_common_api.h>

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

RETRO_END_DECLS

#endif

/* Copyright  (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (matrix.h).
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

#ifndef __LIBRETRO_SDK_GFX_MATH_MATRIX_4X4_H__
#define __LIBRETRO_SDK_GFX_MATH_MATRIX_4X4_H__

/* Column-major matrix (OpenGL-style).
 * Reimplements functionality from FF OpenGL pipeline to be able 
 * to work on GLES 2.0 and modern GL variants.
 */

typedef struct math_matrix_4x4
{
   float data[16];
} math_matrix_4x4;

#define MAT_ELEM_4X4(mat, r, c) ((mat).data[4 * (c) + (r)])

void matrix_4x4_identity(math_matrix_4x4 *mat);
void matrix_4x4_transpose(math_matrix_4x4 *out, const math_matrix_4x4 *in);

void matrix_4x4_rotate_x(math_matrix_4x4 *mat, float rad);
void matrix_4x4_rotate_y(math_matrix_4x4 *mat, float rad);
void matrix_4x4_rotate_z(math_matrix_4x4 *mat, float rad);

void matrix_4x4_ortho(math_matrix_4x4 *mat,
      float left, float right,
      float bottom, float top,
      float znear, float zfar);

void matrix_4x4_multiply(math_matrix_4x4 *out, const math_matrix_4x4 *a, const math_matrix_4x4 *b);

void matrix_4x4_scale(math_matrix_4x4 *out, float x, float y, float z);
void matrix_4x4_translate(math_matrix_4x4 *out, float x, float y, float z);
void matrix_4x4_projection(math_matrix_4x4 *out, float znear, float zfar);

#endif


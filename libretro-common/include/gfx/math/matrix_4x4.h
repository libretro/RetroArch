/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (matrix_4x4.h).
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

#include <retro_inline.h>
#include <retro_common_api.h>

#include <gfx/math/vector_3.h>

/* Column-major matrix (OpenGL-style).
 * Reimplements functionality from FF OpenGL pipeline to be able 
 * to work on GLES 2.0 and modern GL variants.
 */

#define MAT_ELEM_4X4(mat, row, column) ((mat).data[4 * (column) + (row)])

RETRO_BEGIN_DECLS

typedef struct math_matrix_4x4
{
   float data[16];
} math_matrix_4x4;

/*
 * Sets mat to an identity matrix
 */
static INLINE void matrix_4x4_identity(math_matrix_4x4 *mat)
{
   unsigned i;

   MAT_ELEM_4X4(*mat, 0, 1)    = 0.0f;
   MAT_ELEM_4X4(*mat, 0, 2)    = 0.0f;
   MAT_ELEM_4X4(*mat, 0, 3)    = 0.0f;

   MAT_ELEM_4X4(*mat, 1, 0)    = 0.0f;
   MAT_ELEM_4X4(*mat, 1, 2)    = 0.0f;
   MAT_ELEM_4X4(*mat, 1, 3)    = 0.0f;

   MAT_ELEM_4X4(*mat, 2, 0)    = 0.0f;
   MAT_ELEM_4X4(*mat, 2, 1)    = 0.0f;
   MAT_ELEM_4X4(*mat, 2, 3)    = 0.0f;

   MAT_ELEM_4X4(*mat, 3, 0)    = 0.0f;
   MAT_ELEM_4X4(*mat, 3, 1)    = 0.0f;
   MAT_ELEM_4X4(*mat, 3, 2)    = 0.0f;

   for (i = 0; i < 4; i++)
      MAT_ELEM_4X4(*mat, i, i) = 1.0f;
}

void matrix_4x4_copy(math_matrix_4x4 *dst, const math_matrix_4x4 *src);
void matrix_4x4_transpose(math_matrix_4x4 *out, const math_matrix_4x4 *in);

void matrix_4x4_rotate_x(math_matrix_4x4 *mat, float rad);
void matrix_4x4_rotate_y(math_matrix_4x4 *mat, float rad);
void matrix_4x4_rotate_z(math_matrix_4x4 *mat, float rad);

void matrix_4x4_ortho(math_matrix_4x4 *mat,
      float left, float right,
      float bottom, float top,
      float znear, float zfar);

void matrix_4x4_lookat(math_matrix_4x4 *out,
      vec3_t eye,
      vec3_t center,
      vec3_t up);

void matrix_4x4_multiply(math_matrix_4x4 *out, const math_matrix_4x4 *a, const math_matrix_4x4 *b);

void matrix_4x4_scale(math_matrix_4x4 *out, float x, float y, float z);
void matrix_4x4_translate(math_matrix_4x4 *out, float x, float y, float z);
void matrix_4x4_projection(math_matrix_4x4 *out, float y_fov, float aspect, float znear, float zfar);

RETRO_END_DECLS

#endif


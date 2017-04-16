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

#include <math.h>
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
#define matrix_4x4_identity(mat) \
   MAT_ELEM_4X4(mat, 0, 0)    = 1.0f; \
   MAT_ELEM_4X4(mat, 0, 1)    = 0.0f; \
   MAT_ELEM_4X4(mat, 0, 2)    = 0.0f; \
   MAT_ELEM_4X4(mat, 0, 3)    = 0.0f; \
   MAT_ELEM_4X4(mat, 1, 0)    = 0.0f; \
   MAT_ELEM_4X4(mat, 1, 1)    = 1.0f; \
   MAT_ELEM_4X4(mat, 1, 2)    = 0.0f; \
   MAT_ELEM_4X4(mat, 1, 3)    = 0.0f; \
   MAT_ELEM_4X4(mat, 2, 0)    = 0.0f; \
   MAT_ELEM_4X4(mat, 2, 1)    = 0.0f; \
   MAT_ELEM_4X4(mat, 2, 2)    = 1.0f; \
   MAT_ELEM_4X4(mat, 2, 3)    = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 0)    = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 1)    = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 2)    = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 3)    = 1.0f

void matrix_4x4_copy(math_matrix_4x4 *dst, const math_matrix_4x4 *src);
void matrix_4x4_transpose(math_matrix_4x4 *out, const math_matrix_4x4 *in);

/*
 * Builds an X-axis rotation matrix
 */
#define matrix_4x4_rotate_x(mat, radians) \
{ \
   float cosine             = cosf(radians); \
   float sine               = sinf(radians); \
   MAT_ELEM_4X4(mat, 0, 0) = 1.0f; \
   MAT_ELEM_4X4(mat, 0, 1) = 0.0f; \
   MAT_ELEM_4X4(mat, 0, 2) = 0.0f; \
   MAT_ELEM_4X4(mat, 0, 3) = 0.0f; \
   MAT_ELEM_4X4(mat, 1, 0) = 0.0f; \
   MAT_ELEM_4X4(mat, 1, 1) = cosine; \
   MAT_ELEM_4X4(mat, 1, 2) = -sine; \
   MAT_ELEM_4X4(mat, 1, 3) = 0.0f; \
   MAT_ELEM_4X4(mat, 2, 0) = 0.0f; \
   MAT_ELEM_4X4(mat, 2, 1) = sine; \
   MAT_ELEM_4X4(mat, 2, 2) = cosine; \
   MAT_ELEM_4X4(mat, 2, 3) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 0) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 1) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 2) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 3) = 1.0f; \
}

/*
 * Builds a rotation matrix using the 
 * rotation around the Y-axis.
 */

#define matrix_4x4_rotate_y(mat, radians) \
{ \
   float cosine             = cosf(radians); \
   float sine               = sinf(radians); \
   MAT_ELEM_4X4(mat, 0, 0) = cosine; \
   MAT_ELEM_4X4(mat, 0, 1) = 0.0f; \
   MAT_ELEM_4X4(mat, 0, 2) = -sine; \
   MAT_ELEM_4X4(mat, 0, 3) = 0.0f; \
   MAT_ELEM_4X4(mat, 1, 0) = 0.0f; \
   MAT_ELEM_4X4(mat, 1, 1) = 1.0f; \
   MAT_ELEM_4X4(mat, 1, 2) = 0.0f; \
   MAT_ELEM_4X4(mat, 1, 3) = 0.0f; \
   MAT_ELEM_4X4(mat, 2, 0) = sine; \
   MAT_ELEM_4X4(mat, 2, 1) = 0.0f; \
   MAT_ELEM_4X4(mat, 2, 2) = cosine; \
   MAT_ELEM_4X4(mat, 2, 3) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 0) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 1) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 2) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 3) = 1.0f; \
}

/*
 * Builds a rotation matrix using the 
 * rotation around the Z-axis.
 */
#define matrix_4x4_rotate_z(mat, radians) \
{ \
   float cosine             = cosf(radians); \
   float sine               = sinf(radians); \
   MAT_ELEM_4X4(mat, 0, 0) = cosine; \
   MAT_ELEM_4X4(mat, 0, 1) = -sine; \
   MAT_ELEM_4X4(mat, 0, 2) = 0.0f; \
   MAT_ELEM_4X4(mat, 0, 3) = 0.0f; \
   MAT_ELEM_4X4(mat, 1, 0) = sine; \
   MAT_ELEM_4X4(mat, 1, 1) = cosine; \
   MAT_ELEM_4X4(mat, 1, 2) = 0.0f; \
   MAT_ELEM_4X4(mat, 1, 3) = 0.0f; \
   MAT_ELEM_4X4(mat, 2, 0) = 0.0f; \
   MAT_ELEM_4X4(mat, 2, 1) = 0.0f; \
   MAT_ELEM_4X4(mat, 2, 2) = 1.0f; \
   MAT_ELEM_4X4(mat, 2, 3) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 0) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 1) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 2) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 3) = 1.0f; \
}

/*
 * Creates an orthographic projection matrix.
 */
#define matrix_4x4_ortho(mat, left, right, bottom, top, znear, zfar) \
{ \
   float rl                 = (right) - (left); \
   float tb                 = (top)   - (bottom); \
   float fn                 = (zfar)  - (znear); \
   MAT_ELEM_4X4(mat, 0, 0) =  2.0f / rl; \
   MAT_ELEM_4X4(mat, 0, 1) =  0.0f; \
   MAT_ELEM_4X4(mat, 0, 2) =  0.0f; \
   MAT_ELEM_4X4(mat, 0, 3) = -((left) + (right))  / rl; \
   MAT_ELEM_4X4(mat, 1, 0) =  0.0f; \
   MAT_ELEM_4X4(mat, 1, 1) =  2.0f / tb; \
   MAT_ELEM_4X4(mat, 1, 2) =  0.0f; \
   MAT_ELEM_4X4(mat, 1, 3) = -((top)  + (bottom)) / tb; \
   MAT_ELEM_4X4(mat, 2, 0) =  0.0f; \
   MAT_ELEM_4X4(mat, 2, 1) =  0.0f; \
   MAT_ELEM_4X4(mat, 2, 2) = -2.0f / fn; \
   MAT_ELEM_4X4(mat, 2, 3) = -((zfar) + (znear))  / fn; \
   MAT_ELEM_4X4(mat, 3, 0) =  0.0f; \
   MAT_ELEM_4X4(mat, 3, 1) =  0.0f; \
   MAT_ELEM_4X4(mat, 3, 2) =  0.0f; \
   MAT_ELEM_4X4(mat, 3, 3) =  1.0f; \
}

void matrix_4x4_lookat(math_matrix_4x4 *out,
      vec3_t eye,
      vec3_t center,
      vec3_t up);

void matrix_4x4_multiply(math_matrix_4x4 *out, const math_matrix_4x4 *a, const math_matrix_4x4 *b);

#define matrix_4x4_scale(mat, x, y, z) \
   MAT_ELEM_4X4(mat, 0, 0) = x; \
   MAT_ELEM_4X4(mat, 0, 1) = 0.0f; \
   MAT_ELEM_4X4(mat, 0, 2) = 0.0f; \
   MAT_ELEM_4X4(mat, 0, 3) = 0.0f; \
   MAT_ELEM_4X4(mat, 1, 0) = 0.0f; \
   MAT_ELEM_4X4(mat, 1, 1) = y; \
   MAT_ELEM_4X4(mat, 1, 2) = 0.0f; \
   MAT_ELEM_4X4(mat, 1, 3) = 0.0f; \
   MAT_ELEM_4X4(mat, 2, 0) = 0.0f; \
   MAT_ELEM_4X4(mat, 2, 1) = 0.0f; \
   MAT_ELEM_4X4(mat, 2, 2) = z; \
   MAT_ELEM_4X4(mat, 2, 3) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 0) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 1) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 2) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 3) = 1.0f

/*
 * Builds a translation matrix. All other elements in 
 * the matrix will be set to zero except for the
 * diagonal which is set to 1.0
 */

#define matrix_4x4_translate(mat, x, y, z) \
   MAT_ELEM_4X4(mat, 0, 0) = 1.0f; \
   MAT_ELEM_4X4(mat, 0, 1) = 0.0f; \
   MAT_ELEM_4X4(mat, 0, 2) = 0.0f; \
   MAT_ELEM_4X4(mat, 0, 3) = x; \
   MAT_ELEM_4X4(mat, 1, 0) = 0.0f; \
   MAT_ELEM_4X4(mat, 1, 1) = 1.0f; \
   MAT_ELEM_4X4(mat, 1, 2) = 1.0f; \
   MAT_ELEM_4X4(mat, 1, 3) = y; \
   MAT_ELEM_4X4(mat, 2, 0) = 0.0f; \
   MAT_ELEM_4X4(mat, 2, 1) = 0.0f; \
   MAT_ELEM_4X4(mat, 2, 2) = 1.0f; \
   MAT_ELEM_4X4(mat, 2, 3) = z; \
   MAT_ELEM_4X4(mat, 3, 0) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 1) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 2) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 3) = 1.0f

/*
 * Creates a perspective projection matrix.
 */

#define  matrix_4x4_projection(mat, y_fov, aspect, znear, zfar) \
{ \
   float const a            = 1.f / tan((y_fov) / 2.f); \
   float delta_z            = (zfar) - (znear); \
   MAT_ELEM_4X4(mat, 0, 0) = a / (aspect); \
   MAT_ELEM_4X4(mat, 0, 1) = 0.0f; \
   MAT_ELEM_4X4(mat, 0, 2) = 0.0f; \
   MAT_ELEM_4X4(mat, 0, 3) = 0.0f; \
   MAT_ELEM_4X4(mat, 1, 0) = 0.0f; \
   MAT_ELEM_4X4(mat, 1, 1) = a; \
   MAT_ELEM_4X4(mat, 1, 2) = 0.0f; \
   MAT_ELEM_4X4(mat, 1, 3) = 0.0f; \
   MAT_ELEM_4X4(mat, 2, 0) = 0.0f; \
   MAT_ELEM_4X4(mat, 2, 1) = 0.0f; \
   MAT_ELEM_4X4(mat, 2, 2) = -(((zfar) + (znear)) / delta_z); \
   MAT_ELEM_4X4(mat, 2, 3) = -1.f; \
   MAT_ELEM_4X4(mat, 3, 0) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 1) = 0.0f; \
   MAT_ELEM_4X4(mat, 3, 2) = -((2.f * (zfar) * (znear)) / delta_z); \
   MAT_ELEM_4X4(mat, 3, 3) = 0.0f; \
}

RETRO_END_DECLS

#endif


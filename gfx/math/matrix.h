/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#ifndef MATH_MATRIX_H__
#define MATH_MATRIX_H__

// Colunm-major matrix (OpenGL-style).
// Reimplements functionality from FF OpenGL pipeline to be able to work on GLES 2.0 and modern GL variants.
typedef struct math_matrix
{
   float data[16];
} math_matrix;

#define MAT_ELEM(mat, r, c) ((mat).data[4 * (c) + (r)])

void matrix_load_identity(math_matrix *mat);
void matrix_transpose(math_matrix *out, const math_matrix *in);

void matrix_rotate_x(math_matrix *mat, float rad);
void matrix_rotate_y(math_matrix *mat, float rad);
void matrix_rotate_z(math_matrix *mat, float rad);

void matrix_ortho(math_matrix *mat,
      float left, float right,
      float bottom, float top,
      float znear, float zfar);

void matrix_multiply(math_matrix *out, const math_matrix *a, const math_matrix *b);

#endif


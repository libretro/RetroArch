/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2012-2014 - Michael Lelli
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

#ifndef MATH_MATRIX_3X3_H__
#define MATH_MATRIX_3X3_H__

#include "boolean.h"

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

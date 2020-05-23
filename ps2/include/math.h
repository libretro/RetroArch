/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2018 - Francisco Javier Trujillo Mata - fjtrujy
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 * * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MATH_H
#define MATH_H

#include <floatlib.h>
#define roundf(in) (in >= 0.0f ? floorf(in + 0.5f) : ceilf(in - 0.5f))

#define cos(x) ((double)cosf((float)x))
#define pow(x, y) ((double)powf((float)x, (float)y))
#define sin(x) ((double)sinf((float)x))
#define ceil(x) ((double)ceilf((float)x))
#define floor(x) ((double)floorf((float)x))
#define sqrt(x) ((double)sqrtf((float)x))
#define fabs(x) ((double)fabsf((float)(x)))
#define round(x) ((double)roundf((float)(x)))

#define fmaxf(a, b) (((a) > (b)) ? (a) : (b))
#define fminf(a, b) (((a) < (b)) ? (a) : (b))

#define exp(a) ((double)expf((float)a))
#define log(a) ((double)logf((float)a))

#define fmod(a, b) (a - b * floor(a / b));

#endif //MATH_H

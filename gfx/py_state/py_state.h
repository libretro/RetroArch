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

#ifndef __RARCH_PY_STATE_H
#define __RARCH_PY_STATE_H

#include <stdint.h>
#include "../../boolean.h"

#ifndef PY_STATE_OMIT_DECLARATION
typedef struct py_state py_state_t;
#endif

py_state_t *py_state_new(const char *program, unsigned is_file, const char *pyclass);
void py_state_free(py_state_t *handle);

float py_state_get(py_state_t *handle, 
      const char *id, unsigned frame_count);

#endif

/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __BPS_H
#define __BPS_H

#include <stdint.h>
#include <stddef.h>

// BPS implementation from bSNES (nall::).

typedef enum bps_error
{
   BPS_UNKNOWN,
   BPS_SUCCESS,
   BPS_PATCH_TOO_SMALL,
   BPS_PATCH_INVALID_HEADER,
   BPS_SOURCE_TOO_SMALL,
   BPS_TARGET_TOO_SMALL,
   BPS_SOURCE_CHECKSUM_INVALID,
   BPS_TARGET_CHECKSUM_INVALID,
   BPS_PATCH_CHECKSUM_INVALID
} bps_error_t;

bps_error_t bps_apply_patch(
      const uint8_t *patch_data, size_t patch_length,
      const uint8_t *source_data, size_t source_length,
      uint8_t *target_data, size_t *target_length);

#endif


/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef __PATCH_H
#define __PATCH_H

#include <stdint.h>
#include <stddef.h>

// BPS/UPS/IPS implementation from bSNES (nall::).
// Modified for RetroArch.

typedef enum
{
   PATCH_UNKNOWN,
   PATCH_SUCCESS,
   PATCH_PATCH_TOO_SMALL,
   PATCH_PATCH_INVALID_HEADER,
   PATCH_PATCH_INVALID,
   PATCH_SOURCE_TOO_SMALL,
   PATCH_TARGET_TOO_SMALL,
   PATCH_SOURCE_INVALID,
   PATCH_TARGET_INVALID,
   PATCH_SOURCE_CHECKSUM_INVALID,
   PATCH_TARGET_CHECKSUM_INVALID,
   PATCH_PATCH_CHECKSUM_INVALID
} patch_error_t;

typedef patch_error_t (*patch_func_t)(const uint8_t*, size_t, const uint8_t*, size_t, uint8_t*, size_t*);

patch_error_t bps_apply_patch(
      const uint8_t *patch_data, size_t patch_length,
      const uint8_t *source_data, size_t source_length,
      uint8_t *target_data, size_t *target_length);

patch_error_t ups_apply_patch(
      const uint8_t *patch_data, size_t patch_length,
      const uint8_t *source_data, size_t source_length,
      uint8_t *target_data, size_t *target_length);


patch_error_t ips_apply_patch(
      const uint8_t *patch_data, size_t patch_length,
      const uint8_t *source_data, size_t source_length,
      uint8_t *target_data, size_t *target_length);

#endif

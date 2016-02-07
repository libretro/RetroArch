/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

/* BPS/UPS/IPS implementation from bSNES (nall::).
 * Modified for RetroArch. */

typedef enum
{
   PATCH_UNKNOWN = 0,
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

typedef patch_error_t (*patch_func_t)(const uint8_t*, size_t,
      const uint8_t*, size_t, uint8_t*, size_t*);

/**
 * patch_content:
 * @buf          : buffer of the content file.
 * @size         : size   of the content file.
 *
 * Apply patch to the content file in-memory.
 *
 **/
void patch_content(uint8_t **buf, ssize_t *size);

#endif

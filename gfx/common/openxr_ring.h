/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

#ifndef __OPENXR_RING_H
#define __OPENXR_RING_H

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define OPENXR_RING_SIZE 3

typedef struct openxr_ring
{
   retro_atomic_int_t middle;
   int back;  /* producer only */
   int front; /* consumer only */
} openxr_ring_t;

void openxr_ring_init(openxr_ring_t *r);

/* Producer: publish the back record and acquire the next writable one. */
int openxr_ring_back(const openxr_ring_t *r);
int openxr_ring_publish(openxr_ring_t *r);

/* Consumer: acquire the newest record, if any. */
bool openxr_ring_take(openxr_ring_t *r, int *front);
int openxr_ring_front(const openxr_ring_t *r);

RETRO_END_DECLS

#endif

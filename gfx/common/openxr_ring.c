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

#include "openxr_ring.h"

#define OPENXR_RING_NEW 4
#define OPENXR_RING_INDEX 3

void openxr_ring_init(openxr_ring_t *r)
{
   r->back  = 0;
   r->front = 2;
   retro_atomic_int_init(&r->middle, 1);
}

int openxr_ring_back(const openxr_ring_t *r)
{
   return r->back;
}

int openxr_ring_publish(openxr_ring_t *r)
{
   r->back = retro_atomic_exchange_int(&r->middle,
         r->back | OPENXR_RING_NEW) & OPENXR_RING_INDEX;
   return r->back;
}

bool openxr_ring_take(openxr_ring_t *r, int *front)
{
   if (!(retro_atomic_load_acquire_int(&r->middle) & OPENXR_RING_NEW))
      return false;
   r->front = retro_atomic_exchange_int(&r->middle,
         r->front) & OPENXR_RING_INDEX;
   *front = r->front;
   return true;
}

int openxr_ring_front(const openxr_ring_t *r)
{
   return r->front;
}

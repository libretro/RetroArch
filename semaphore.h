/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015 - Andre Leiradella
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

#ifndef __RARCH_SEMAPHORE_H
#define __RARCH_SEMAPHORE_H

typedef struct ssem ssem_t;

/**
 * ssem_create:
 * @value                   : initial value for the semaphore
 *
 * Create a new semaphore.
 *
 * Returns: pointer to new semaphore if successful, otherwise NULL.
 */
ssem_t *ssem_new(int value);

void ssem_free(ssem_t *semaphore);

void ssem_wait(ssem_t *semaphore);

void ssem_signal(ssem_t *semaphore);

#endif /* __RARCH_SEMAPHORE_H */
